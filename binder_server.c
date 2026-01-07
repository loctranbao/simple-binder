#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <string.h>
#include "simple_binder.h"


#define TAG "BINDER_SERVER"
#define SERVICE_NAME "echo"
const char* champion = "-CHAMPION";
int fd;

void serve_transaction(struct binder_transaction* transaction) {
	printf("%s cmd = %d\n", __func__, transaction->cmd);
	switch (transaction->cmd) {
	case 0: // echo + champion
		{
			strcpy(transaction->reply_ptr, transaction->input_ptr);
			strcat(transaction->reply_ptr, champion);
			transaction->reply_size = strlen(transaction->reply_ptr);
			unsigned int src = transaction->src;
			unsigned int target = transaction->target;

			transaction->src = target;
			transaction->target = src;

			if (ioctl(fd, IOCTL_BINDER_TRANSACT_REPLY , transaction) == -1) {
		        perror("ioctl(IOCTL_BINDER_TRANSACT_REPLY) error");
		    }

			break;
		}
	default:
		printf("%s unsupported method %d\n", __func__, transaction->cmd);
	}
}

int main(){
	printf("%s started\n", TAG);

	fd = open("/dev/simple_binder", O_RDONLY, 0644);
	if(fd < 0) {
		perror("fail to open /dev/simple_binder");
		goto exit;
	}
	int handle = -1;
	if (ioctl(fd, IOCTL_REQUEST_NEW_BINDER , &handle) == -1) {
        perror("ioctl(IOCTL_REQUEST_NEW_BINDER) error");
        goto ioctl_err;
    }

    printf("%s get new binder handle from kernel %d\n", TAG, handle);

    // struct binder_service_info service = {
    // 	.id = handle,
    // 	.name_len = strlen(SERVICE_NAME),
    // 	.name = SERVICE_NAME,
    // };


    struct binder_transaction transaction = {
    	.cmd = IOCTL_REGISTER_BINDER,
    	.input_ptr = SERVICE_NAME,
    	.input_size = strlen(SERVICE_NAME),
    	.target = 0, //simple_binder ctx_mgr
    	.src = handle
    };

	if (ioctl(fd, IOCTL_BINDER_TRANSACT , &transaction) == -1) {
        perror("ioctl(IOCTL_BINDER_TRANSACT) error");
        goto ioctl_err;
    }

    
    while(1){
    	memset(&transaction, 0 , sizeof(transaction));
		if (ioctl(fd, IOCTL_ENTER_LOOP , &transaction) == -1) {
	        perror("ioctl(IOCTL_ENTER_LOOP) error");
	        goto ioctl_err;
	    }
	    
	    serve_transaction(&transaction);
    }



ioctl_err:
	close(fd);
exit:
	return 0;
}