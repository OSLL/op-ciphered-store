/*
 ===============================================================================
 Driver Name		:		cryptochr
 Author			    :		Igor Berezhnoy
 License			:		GPL
 Description		:		simple encrypting char device driver
 ===============================================================================
 */

#include"cryptochr.h"

#define CRYPTOCHR_N_MINORS 1
#define CRYPTOCHR_FIRST_MINOR 0
#define CRYPTOCHR_NODE_NAME "your_cool_crypto_device"
#define CRYPTOCHR_BUFF_SIZE 2048
#define LOCK 0
#define UNLOCK 1

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Igor Berezhnoy");

// static ulong message_life_timeout;

static char *message, *origin;

ushort message_size = 0;

int cryptochr_major = 0;

static bool encrypted = false;

static dev_t cryptochr_d;

struct class *cryptochr_class;

typedef struct privatedata {
    
        int minor;

        struct cdev cdev;

        struct device *cryptochr_device;


} cryptochr_private;

static cryptochr_private cryptochr_device;

static void encrypt(char *string, const char *key) {
        
        size_t length = strlen(key), i = 0;
        
        while (*string) {
                *string++ ^= key[i++ % length];
        }
    }

static int cryptochr_open(struct inode *inode, struct file *filp) {

        PDEBUG("Called open() function in cryptochr\n");

        return 0;
    }

static int cryptochr_release(struct inode *inode, struct file *filp) {

        PDEBUG("Called release() function in cryptochr\n");

        return 0;
    }

static ssize_t cryptochr_read(struct file *filp, char __user *ubuff,size_t count,loff_t *offp) {       
        PDEBUG("Called read() function in cryptochr\n");
        if (!encrypted) {
                int n = 0;
                
                while (count && *origin) {
                        put_user(*(origin++), ubuff++);
                        count--;
                        n++;
                }

                return n;
        }
        return 0;
}

static ssize_t cryptochr_write(struct file *filp, const char __user *ubuff, size_t count, loff_t *offp) {
        PDEBUG("Called write() function in cryptochr\n");
        
        int res;
        ushort i=0;

        res = copy_from_user(message, ubuff, count-1);
        /*  TODO check res */
        message[count] = '\0';
        message_size = count;
        
        /* Actually splits passed string */
        char *found, *command, *key, *msg;
        char *m = kstrdup(message, GFP_KERNEL);
        while( (found = strsep(&m," ")) != NULL ) {
                    i++;
                    switch (i) {
                        case 1:
                            command = found;
                            break;
                        case 2:
                            key = found;
                            break;
                        case 3:
                            msg = found;
                            break;
                    };
            }
        /* recognize command */
        if (strcmp("encrypt", command) == 0) {
                PDEBUG("encrypt called");
                encrypt(msg, key);
                encrypted = true;
                strcpy(origin, msg);
                PDEBUG("message: %s\n", origin);
                PDEBUG("key: %s\n", key);
                
                
        }
        else if (strcmp("decrypt", command) == 0) {
            PDEBUG("decrypt called");
            PDEBUG("message: %s\n", origin);
            encrypt(origin, key);
            encrypted = false;
            PDEBUG("message: %s\n", origin);
            PDEBUG("key: %s\n", key);
        }
        // PDEBUG("command: %s, ket: %s, msg: %s", command, key, msg);
        
        kfree(m);   
        return count;
    }        
    
static const struct file_operations cryptochr_fops = {
    
	    .owner = THIS_MODULE,
	    .open = cryptochr_open,
	    .release = cryptochr_release,
	    .read = cryptochr_read,
	    .write = cryptochr_write,
};

static int __init cryptochr_init(void)
{
        int i;
        int res;

        res = alloc_chrdev_region(&cryptochr_d,CRYPTOCHR_FIRST_MINOR,CRYPTOCHR_N_MINORS ,DRIVER_NAME);
        if(res) {
                PERR("register device no failed\n");
                return -1;
        }

        cryptochr_major = MAJOR(cryptochr_d);

        cryptochr_class = class_create(THIS_MODULE , DRIVER_NAME);
        if(!cryptochr_class) {
                PERR("class creation failed\n");
                return -1;
        }

        cryptochr_d = MKDEV(cryptochr_major ,CRYPTOCHR_FIRST_MINOR+i);

        cdev_init(&cryptochr_device.cdev , &cryptochr_fops);

        cdev_add(&cryptochr_device.cdev,cryptochr_d,1);

        cryptochr_device.cryptochr_device = device_create(cryptochr_class , NULL ,cryptochr_d, NULL ,CRYPTOCHR_NODE_NAME);

        if(!cryptochr_device.cryptochr_device) {

                class_destroy(cryptochr_class);
                PERR("device creation failed\n");
                return -1;
        }

        /* Allocate the buffer */
        message = kmalloc(CRYPTOCHR_BUFF_SIZE, GFP_KERNEL);
        origin = kmalloc(CRYPTOCHR_BUFF_SIZE, GFP_KERNEL);
        
        cryptochr_device.minor = CRYPTOCHR_FIRST_MINOR;

        PDEBUG("initialization completed\n");

        return 0;
}

static void __exit cryptochr_exit(void)
{
        PDEBUG("EXIT\n");
        cryptochr_d = MKDEV(cryptochr_major, CRYPTOCHR_FIRST_MINOR);

        /* Deallocate the buffer */
        kfree(message);
        kfree(origin);
        cdev_del(&cryptochr_device.cdev);

        device_destroy(cryptochr_class ,cryptochr_d);

        class_destroy(cryptochr_class);

        unregister_chrdev_region(cryptochr_d ,CRYPTOCHR_N_MINORS);

}

module_init(cryptochr_init);

module_exit(cryptochr_exit);

// module_param(message_life_timeout, ulong, S_IRUGO);

