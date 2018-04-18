/*
 ===============================================================================
 Driver Name		:		cryptochr
 Author			    :		Igor Berezhnoy
 License			:		GPL
 Description		:		simple encrypting char device driver
 ===============================================================================
 */

#include"cryptochr.h"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Igor Berezhnoy");

static ulong message_life_timeout;

/* raw message from userspace */
static char *message;

/*buffer to keep decrypted message for a certain time */
static char *readbuf;

ushort buflen = 0;

static uint tcounter = 0;

int cryptochr_major = 0;

static struct kmessage {
    
        char *message;
        char *id;
        char *key;
        ushort message_len;
        bool allow_read;
        struct list_head list;
};

static struct hrtimer hr_timer;

static dev_t cryptochr_d;

struct class *cryptochr_class;

static bool is_encrypt(char *command) {
        const char *string = "encrypt"; 
        return (strcmp(command, string) == 0);
}

static bool is_decrypt(char *command) {
        const char *string = "decrypt"; 
        return (strcmp(command, string) == 0);
}

/* Declare a temporary node pointer for use in different cases */
static struct kmessage *tmp_kmesg;

/* Declare the head of the list */
static struct list_head msg_head;

/* Declare a temporary list_head for traversal & deletion */
static struct list_head *pos;

typedef struct privatedata {
    
        int minor;

        struct cdev cdev;

        struct device *cryptochr_device;


} cryptochr_private;

static cryptochr_private cryptochr_device;

static void encrypt(char *string, char *key) {
        
        size_t length = strlen(key), i = 0;
        
        while (*string) {
                *string++ ^= key[i++ % length];
        }
    }
    
enum hrtimer_restart lock(struct hrtimer *timer) {
            /* encrypt it back as timer is finished */
            strcpy(readbuf, '\0');
            
            return HRTIMER_NORESTART;
            
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
        int n = 0;
                
        if (NULL != readbuf) {
            while (count && *readbuf) {
                    put_user(*(readbuf++), ubuff++);
                    count--;
                    n++;
            }

            return n;
        }
        else return 0;
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
        char *found, *command, *key, *msg, *id;
        //char *m = kstrdup(message, GFP_KERNEL);
        while( (found = strsep(&message," ")) != NULL ) {
                    i++;
                    switch (i) {
                        /* encrypt key message id
                         * decrypt key id
                         */
                    case 1:
                            command = found;
                            break;
                    case 2:
                            key = found;
                            break;
                    case 3:
                            if (is_decrypt(command)) 
                                    id = found;
                            else if (is_encrypt(command)) 
                                    msg = found;
                            else
                                    PERR("Wrong command '%s'\n", command);
                                    return -EINVAL;
                            break;
                    case 4:
                            id = found;
                            break;
                    default:
                            PERR("Passed redundant parameter: %s", found);
                    };
            }

        /* recognize command */
        if (is_encrypt(command)) {
                PDEBUG("encrypt called");
                /* Creates a node, fill its fileds and encrypts message*/
                tmp_kmesg = kmalloc(sizeof(struct kmessage), GFP_KERNEL);
                
                tmp_kmesg->message = kmalloc(MSG_BUFF_SIZE, GFP_KERNEL);
                strcpy(tmp_kmesg->message, msg);
                
                tmp_kmesg->key = kmalloc(KEY_BUFF_SIZE, GFP_KERNEL);
                strcpy(tmp_kmesg->key, key);
                
                tmp_kmesg->id = kmalloc(ID_BUFF_SIZE, GFP_KERNEL);
                strcpy(tmp_kmesg->id, id);
                
                encrypt(tmp_kmesg->message, tmp_kmesg->key);
                //tmp_kmesg->allow_read = false;
                
                /* Add the node to the tail of the list */
                list_add_tail(&tmp_kmesg->list, &msg_head);
                PDEBUG("Created kmessage, memory used: %ld\n", sizeof(tmp_kmesg));
                
                // PDEBUG("ID: %s\nEnrypted message: %s\nused key: %s\n", id, msg, key);
        }
        
        else if (is_decrypt(command)) {
                PDEBUG("decrypt called");
                uint found = 0;
                struct list_head *next;
                list_for_each_safe(pos, next, &msg_head) {
                    
                        tmp_kmesg = list_entry(pos, struct kmessage, list);
                        
                        if (strcmp(tmp_kmesg->id, id) == 0) {
                                found++;
                                encrypt(tmp_kmesg->message, key);
                                //tmp_kmesg->allow_read = true;
                                
                                /* copy decrypted message to read buffer */
                                strcpy(readbuf, tmp_kmesg->message);
                                
                                /* set timer */
                                ktime_t ktime;
                                ktime = ktime_set(0, MS_TO_NS(message_life_timeout));
                                hrtimer_init(&hr_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
                                hr_timer.function = &lock;
                                hrtimer_start(&hr_timer, ktime, HRTIMER_MODE_REL);
                                break;
                        }
                }
                if (!found) {
                    PERR("No such ID: %s\n", id);
                    return -EINVAL;
                }
        
        // kfree(m);   
        return count;
    }        
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
                PERR("device registration failed\n");
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
        
        /* Initialise the list with the head variable declared above */
        INIT_LIST_HEAD(&(msg_head));
        
        /* Allocate the buffer */
        message = kmalloc(CRYPTOCHR_BUFF_SIZE, GFP_KERNEL);
        readbuf = kmalloc(MSG_BUFF_SIZE, GFP_KERNEL);
        
        cryptochr_device.minor = CRYPTOCHR_FIRST_MINOR;

        PDEBUG("initialization completed\n");

        return 0;
}
    
static void __exit cryptochr_exit(void)
{
        PDEBUG("EXIT\n");
        
        struct list_head *next;
        
        cryptochr_d = MKDEV(cryptochr_major, CRYPTOCHR_FIRST_MINOR);

        /* Deallocate the buffer */
        kfree(message);
        kfree(readbuf);
        
        /** Traverse the list & delete the nodes. As we are about to delete the current
        *  node one by one, we need to preserve the next node's addess. Hence a pointer
        *  named 'next' is taken.
        */
        list_for_each_safe(pos, next, &msg_head) {

                /* Extract the node address of the current node */
                tmp_kmesg = list_entry(pos, struct kmessage, list);

                /* Delete the current node */
                kfree(tmp_kmesg->message);
                kfree(tmp_kmesg->key);
                kfree(tmp_kmesg->id);
                kfree(tmp_kmesg);
        }
        
        cdev_del(&cryptochr_device.cdev);

        device_destroy(cryptochr_class ,cryptochr_d);

        class_destroy(cryptochr_class);

        unregister_chrdev_region(cryptochr_d ,CRYPTOCHR_N_MINORS);

}

module_init(cryptochr_init);

module_exit(cryptochr_exit);

module_param(message_life_timeout, ulong, S_IRUGO);

