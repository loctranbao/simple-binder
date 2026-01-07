#ifndef _SIMPLE_BINDER_
#define _SIMPLE_BINDER_

#define IOCTL_REGISTER_CTX_MANAGER 		989890
#define IOCTL_ENTER_LOOP           		989891
#define IOCTL_REQUEST_NEW_BINDER   		989892
#define IOCTL_REGISTER_BINDER      		989893
#define IOCTL_REGISTER_BINDER_REPLY     989894
#define IOCTL_GET_BINDER           		989895
#define IOCTL_GET_BINDER_REPLY     		989896
#define IOCTL_BINDER_TRANSACT      		989897
#define IOCTL_BINDER_TRANSACT_REPLY     989898

struct binder_transaction {
	/*kinda like ioctl command*/
	unsigned int cmd;

	/*pointer to input buffer, contain arguments used for ioctl command*/
	char input_ptr[256];

	/*size of the input buffer in bytes*/
	unsigned int input_size;

	/*pointer to reply buffer, contain return result of ioctl command*/
	char reply_ptr[256];

	/*size of the reply buffer in bytes*/
	unsigned int reply_size;

	/*id of target binder node*/
	unsigned int target;

	/*id of source binder node*/
	unsigned int src;
};

#endif
