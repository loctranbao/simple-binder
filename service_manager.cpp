#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <cstring>
#include <string>
#include <unordered_map>

extern "C" {
	#include "simple_binder.h"
}

#define TAG "SERVICE_MANAGER"

// temporary use a global map to store binder service name and its binder id
// use singleton to store a map would be a better approach
std::unordered_map<std::string, unsigned int> services;
int simple_binder_fd;

void handle_binder_transaction(struct binder_transaction* transaction) {
	printf("%s cmd = %d\n", __func__, transaction->cmd);
	switch (transaction->cmd) {
	case IOCTL_REGISTER_BINDER:
		{
			transaction->cmd = IOCTL_REGISTER_BINDER_REPLY;
			char* service_name = (char*)transaction->input_ptr;
			unsigned int binder_id = transaction->src;
			int* reply_info = (int*)transaction->reply_ptr;
			transaction->reply_size = sizeof(int);
			transaction->target = transaction->src;
			transaction->src = 0;//ctx_mgr
			*reply_info = (services.find((service_name)) != services.end()) ? -1 : 0;
			if (ioctl(simple_binder_fd, IOCTL_BINDER_TRANSACT_REPLY , transaction) == -1) {
		        perror("ioctl(IOCTL_REGISTER_BINDER_REPLY) error");
		        return;
		    }

			if(services.find(service_name) != services.end()) {
				printf("service %s already registered\n", service_name);
				return;
			}			
			services[service_name] = binder_id;
		

			break;
		}
	case IOCTL_GET_BINDER:
		{
			char* service_name = (char*)transaction->input_ptr;
			transaction->cmd = IOCTL_GET_BINDER_REPLY;
			int* reply_info = (int*)transaction->reply_ptr;
			*reply_info = -1;
			transaction->reply_size = sizeof(int);
			transaction->target = transaction->src;
			transaction->src = 0;//ctx_mgr
			if(services.find(service_name) != services.end()) {
				*reply_info = services[service_name];
			}

			if (ioctl(simple_binder_fd, IOCTL_BINDER_TRANSACT_REPLY , transaction) == -1) {
		        perror("ioctl(IOCTL_GET_BINDER_REPLY) error");
		        return;
		    }


			break;
		}
	default:
		printf("unknown binder cmd %d\n", transaction->cmd);
		break;
	}
}

int main(){

	simple_binder_fd = open("/dev/simple_binder", O_RDONLY, 0644);
	if(simple_binder_fd < 0) {
		perror("fail to open /dev/simple_binder");
		goto exit;
	}

	if (ioctl(simple_binder_fd, IOCTL_REGISTER_CTX_MANAGER , NULL) == -1) {
        perror("ioctl(IOCTL_REGISTER_CTX_MANAGER) error");
        goto ioctl_err;
    }

    struct binder_transaction transaction;
    
    while(1){
    	memset(&transaction, 0 , sizeof(transaction));
		if (ioctl(simple_binder_fd, IOCTL_ENTER_LOOP , &transaction) == -1) {
	        perror("ioctl(IOCTL_ENTER_LOOP) error");
	        goto ioctl_err;
	    }
	    handle_binder_transaction(&transaction);
    }

ioctl_err:
	close(simple_binder_fd);
exit:
	return 0;
}