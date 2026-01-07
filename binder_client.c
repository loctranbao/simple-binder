#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <string.h>
#include "simple_binder.h"

#define TAG "BINDER_CLIENT"
#define BINDER_NAME "echo"
#define MSG  "ARSENAL"

int main(){
	int fd = open("/dev/simple_binder", O_RDONLY, 0644);
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

    struct binder_transaction get_binder_transaction = {
	    .cmd = IOCTL_GET_BINDER, // method id
	   	.input_ptr = BINDER_NAME,
	    .input_size = strlen(BINDER_NAME),
	    .target = 0,
	    .src = handle  	
    };    

    
	if (ioctl(fd, IOCTL_BINDER_TRANSACT , &get_binder_transaction) == -1) {
        perror("ioctl(IOCTL_GET_BINDER) error");
        goto ioctl_err;
    }

    int target = *((int*) get_binder_transaction.reply_ptr);
    if(target == -1) {
    	printf("can not get binder_service %s\n", BINDER_NAME);
    	goto ioctl_err;
    }

    printf("echo id = %d\n", target);

    struct binder_transaction transaction = {
	    .cmd = 0, // method id
	   	.input_ptr = MSG,
	    .input_size = strlen(MSG),
	    .target = target,
	    .src = handle  	
    };

    if (ioctl(fd, IOCTL_BINDER_TRANSACT , &transaction) == -1) {
        perror("ioctl(IOCTL_REQUEST_NEW_BINDER) error");
        goto ioctl_err;
    }
    printf("reply from %s binder node for method 0 = %s\n", BINDER_NAME, transaction.reply_ptr);


ioctl_err:
	close(fd);
exit:
	return 0;
}