#include <linux/init.h> 
#include <linux/module.h> 
#include <linux/kernel.h>
#include <linux/cdev.h>
#include <linux/slab.h>
#include <linux/idr.h>
#include <linux/list.h>
#include <linux/mutex.h>
#include "simple_binder.h"


struct binder_work {
    struct binder_transaction* transaction;
    struct list_head transaction_list;
};

struct binder_proc {
    /*current thread that binder proc belong to*/
    struct task_struct* task;

    /*use to put this binder proc sleep/wakeup*/
    wait_queue_head_t wq;

    struct mutex work_list_mutex;
    struct list_head work_list;

};

struct binder_node {
    /*every existed binder node have a unique identifier*/
    unsigned int id;
    
    /*pointer to parent binder proc*/
    struct binder_proc* proc;
    
};

struct binder_ref {
    /*every existed binder ref of a binder proc have a unique identifier*/
    unsigned int handle;

    /*pointer to the binder node that it refer to*/
    struct binder_node* node;
};


struct binder_list {
    struct binder_node* node;
    struct list_head list;
};

//*******************Utility***************************************************//
static void entry_info(struct binder_list * entry);
static void entry_free(struct binder_list * entry);
static void binder_list_for_each(struct list_head *head, void (*func)(struct binder_list *));
static struct binder_proc* proc_by_binder_id(unsigned int binder_id);
static int wait_for_work(struct binder_proc* proc, unsigned int cmd, unsigned long user_ptr);
//*******************Utility***************************************************//



//*********************Simple_binder_global_data*********************************//
DEFINE_IDA(my_device_ida);
struct mutex binder_node_list_mutex;
LIST_HEAD(binder_node_list);
static struct binder_node* ctx_mgr = NULL;
//*********************Simple_binder_global_data*********************************//



static int simple_binder_open(struct inode *, struct file * _file) {
    struct binder_proc* proc ;
    proc = kmalloc(sizeof(struct binder_proc), GFP_KERNEL);
    if(!proc) {
        printk(KERN_ALERT "kmalloc fail to create binder_proc");
        return -1;
    }
    proc->task = current;
    init_waitqueue_head(&proc->wq);
    INIT_LIST_HEAD(&proc->work_list);
    _file->private_data = proc;

    printk(KERN_ALERT "&proc %p, proc %p", &proc, proc);
    return 0;
}


static long simple_binder_ioctl(struct file * _file, unsigned int cmd, unsigned long user_ptr){
    switch (cmd) {
    case IOCTL_REGISTER_CTX_MANAGER:
        {
            if(ctx_mgr) {
                printk(KERN_ALERT "a context manager already registered");
                return -1;
            }
            ctx_mgr = kmalloc(sizeof(struct binder_node), GFP_KERNEL);
            ctx_mgr->id = ida_alloc_min(&my_device_ida, 0, GFP_KERNEL);
            ctx_mgr->proc = _file->private_data;
            printk(KERN_ALERT "ctx_mgr binder node created with id = %d", ctx_mgr->id);
            break;
        }
    case IOCTL_ENTER_LOOP:
        {
            struct binder_proc* proc = _file->private_data;
            wait_for_work(proc, cmd, user_ptr);
            break;
        }
    case IOCTL_REQUEST_NEW_BINDER:
        {   
            struct binder_node* node = kmalloc(sizeof(struct binder_node), GFP_KERNEL);
            node->id = ida_alloc_min(&my_device_ida, 0, GFP_KERNEL);
            node->proc = _file->private_data;

            struct binder_list* list_node = kmalloc(sizeof(struct binder_list), GFP_KERNEL);
            list_node->node = node;
            /*[TODO!] need to revisit to look at why kernel chose this style of API*/
            mutex_lock(&binder_node_list_mutex);
            list_add(&list_node->list, &binder_node_list);
            mutex_unlock(&binder_node_list_mutex);
            if (copy_to_user((int*)user_ptr, &node->id, sizeof(unsigned int))) {
                printk(KERN_ALERT "fail to copy id to user_ptr\n");
                return -EFAULT;
            }
            break;
        }
    case IOCTL_BINDER_TRANSACT:
    case IOCTL_BINDER_TRANSACT_REPLY:
        {
            struct binder_transaction* transaction = kmalloc(sizeof(struct binder_transaction), GFP_KERNEL); ;
            if (copy_from_user(transaction, (struct binder_transaction*)user_ptr, sizeof(struct binder_transaction))) {
                printk(KERN_ALERT "fail to copy binder_service from user_ptr\n");
                return -EFAULT;
            }

            unsigned int target_id = transaction->target;
            struct binder_proc* proc = target_id == 0 ? ctx_mgr->proc : proc_by_binder_id(target_id);

            // push work to callee proc work queue and wake it up
            struct binder_work* work = kmalloc(sizeof(struct binder_work), GFP_KERNEL);
            work->transaction = transaction;
            mutex_lock(&proc->work_list_mutex); 
            list_add(&work->transaction_list, &(proc->work_list));
            mutex_unlock(&proc->work_list_mutex);
            wake_up_interruptible(&(proc->wq));

            if(cmd == IOCTL_BINDER_TRANSACT) {
                struct binder_proc* proc = _file->private_data;
                wait_for_work(proc, cmd, user_ptr);                
            }
            break;
        }    

    default:
        printk(KERN_ALERT "assert simple-binder unknown command");
    }
    return 0;
}

static int simple_binder_mmap(struct file *, struct vm_area_struct *) {
    return 0;
}

static struct cdev c_dev;
static const struct file_operations f_ops = {
    .owner = THIS_MODULE,
    .unlocked_ioctl = simple_binder_ioctl,
    .mmap = simple_binder_mmap,
    .open = simple_binder_open
};
static dev_t dev;
static struct class* clazz;
static struct device* devize;
static int simple_binder_init(void){
    // allocate char device dev_t struct hold device major and minor number
    int error = alloc_chrdev_region(&dev, 0, 1, "simple_binder");
    if(error != 0) {
        printk(KERN_ALERT "fail to allocate device number error %d", error);
        goto alloc_error;
    }

    // initialize cdev struct and register it to kernel global device mapping via cdev_add
    cdev_init(&c_dev, &f_ops);
    c_dev.owner = THIS_MODULE;
    error = cdev_add(&c_dev, dev, 1);
    if(error != 0) {
        printk(KERN_ALERT "fail to add device error %d", error);
        goto cdev_add_error;
    }

    // create driver node in sys/class and /dev
    clazz = class_create("simple_binder_class");
    if(IS_ERR(clazz)) {
        printk(KERN_ALERT "class_create have failed");
        error = PTR_ERR(clazz);
        goto class_create_error;
    }
    devize = device_create(clazz, NULL, dev, NULL, "simple_binder");
    if(IS_ERR(devize)) {
        printk(KERN_ALERT "device_create have failed");
        error = PTR_ERR(devize);
        goto device_create_error;
    }

    mutex_init(&binder_node_list_mutex);


    return 0;
device_create_error:
    class_destroy(clazz);
class_create_error:
    cdev_del(&c_dev);
cdev_add_error:
    unregister_chrdev_region(dev, 1);
alloc_error:
    return error;
}


static void simple_binder_exit(void){
    printk(KERN_ALERT "simple_binder_exit goodbye");

    mutex_lock(&binder_node_list_mutex);
    binder_list_for_each(&binder_node_list, entry_free);
    mutex_unlock(&binder_node_list_mutex);
    mutex_destroy(&binder_node_list_mutex);
    if(ctx_mgr) {
        kfree(ctx_mgr);
    }
    ida_destroy(&my_device_ida);
    device_destroy(clazz, dev);
    class_destroy(clazz);
    cdev_del(&c_dev);
    unregister_chrdev_region(dev, 1);
}

module_init(simple_binder_init);
module_exit(simple_binder_exit);

MODULE_LICENSE("GPL"); 
MODULE_AUTHOR("chenlog"); 
MODULE_DESCRIPTION("Simple Binder IPC");


// **************************************Utility******************************************* //

static void entry_info(struct binder_list * entry) {
    printk(KERN_ALERT "%s {%p}", __func__, entry->node);
}

static void entry_free(struct binder_list * entry) {
    printk(KERN_ALERT "%s free {%p}", __func__, entry->node);
    kfree(entry->node);
    kfree(entry);
}

static void binder_list_for_each(struct list_head *head, void (*func)(struct binder_list *)) {
    struct binder_list *entry;

    // Check if the list is empty first
    if (list_empty(head)) {
        return;
    }

    list_for_each_entry(entry, head, list) {
        func(entry);
    }    
}

static struct binder_proc* proc_by_binder_id(unsigned int binder_id) {
    struct binder_list *entry;
    struct list_head *head = &binder_node_list;

    // Check if the list is empty first
    if (list_empty(head)) {
        return NULL;
    }

    list_for_each_entry(entry, head, list) {
        if(entry->node->id == binder_id) {
            return entry->node->proc;
        }
    }

    return NULL;          
}

static int wait_for_work(struct binder_proc* proc, unsigned int cmd, unsigned long user_ptr) {
    if (wait_event_interruptible(proc->wq, !list_empty(&proc->work_list))) {
        printk(KERN_ALERT "ain't suppose to enter here"); 
    }
    struct binder_work* work;
    long err = 0;
    mutex_lock(&proc->work_list_mutex);     
    work = list_first_entry_or_null(&proc->work_list, struct binder_work, transaction_list);
    if (work) {
        list_del(&work->transaction_list);
        struct binder_transaction* transaction = work->transaction;
        struct binder_transaction __user * user_transaction = (struct binder_transaction*) user_ptr;
        err = copy_to_user(user_transaction, transaction, sizeof(struct binder_transaction));
        kfree(work->transaction);
        kfree(work);

    }
    mutex_unlock(&proc->work_list_mutex);

    return err;        
}
