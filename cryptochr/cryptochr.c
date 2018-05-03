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

static unsigned long message_life_timeout;

static struct kobject *cool_dev_kobj;

/*buffer to keep decrypted message for a certain time */
static char *readbuf;

unsigned short buflen = 0;

int cryptochr_major = 0;

struct kmessage {
    
        char *message;
        char *id;
        unsigned short message_len;
        bool allow_read;
        struct list_head list;
};

struct kobs {
        char *message;
        struct kobject *self;
        struct hrtimer timer;
        struct list_head list;
};

static struct hrtimer hr_timer;

static bool hr_timer_on;

static struct mutex timer_lock;

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

/* Declare the head of the list */
static struct list_head msg_head;

static struct list_head kob_head;

static struct mutex kob_list_lock;

/* Declare a temporary list_head for traversal & deletion */
static struct list_head *pos;

typedef struct privatedata {
        int minor;
        struct cdev cdev;
        struct device *cryptochr_device;
} cryptochr_private;

static cryptochr_private cryptochr_device;

static void encrypt(char *string, const char *key, const size_t Max) {
        
        size_t length = strlen(key), i = 0;
        
        while (i < Max) {
                *string++ ^= key[i++ % length];
        }
}
    
enum hrtimer_restart lock(struct hrtimer *timer) {
            /* erase the message as timer is over */
        PDEBUG("Called lock() function in cryptochr %s\n", readbuf);
        
        mutex_lock(&timer_lock);
        readbuf[0]='\0';
        hr_timer_on = false;
        mutex_unlock(&timer_lock);
                
        return HRTIMER_NORESTART;
            
}


static ssize_t message_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
        struct list_head *next;
        struct kobs *tmp;
        bool found = false;
        ssize_t rv = 0;
        
        /* erase the message as timer is over */
        PDEBUG("Entered message_show call\n");
        
        mutex_lock(&kob_list_lock);
        
        list_for_each_safe(pos, next, &kob_head) {
            
                tmp = list_entry(pos, struct kobs, list);
                if( tmp->self == kobj) {
                        found = true;
                        break;
                        
                    }
        }
        
        if( found) {
                rv = sprintf(buf, "%s\n", tmp->message);
        } 
        else {
                buf[0]='\0';
                rv = 0;
        }
        
        mutex_unlock(&kob_list_lock);
        
        return rv;
}

static struct kobj_attribute o_attr= __ATTR_RO(message);


static void removeKOB_from_list( struct kobs *tmp) {
    
        PDEBUG("Entered removeKOB_from_list call when message=%s\n", tmp->message);
        
        kfree(tmp->message);
        kfree(tmp);
}

static void unlinkObj(  struct kobs *o) {
    
        PDEBUG("Called unlinkObj() function in cryptochr %s\n", kobject_name(o->self) );
        sysfs_remove_file(o->self, &o_attr.attr);
        kobject_put(o->self);
}

enum hrtimer_restart del_ko(struct hrtimer *timer) {
    
        struct kobs *tmp;
        
        PDEBUG("Called del_ko() function in cryptochr \n");
        
        mutex_lock(&kob_list_lock);
        
        tmp = container_of( timer, struct kobs, timer);
        if( NULL != tmp) {
            
                list_del( &tmp->list);
                unlinkObj( tmp);
                removeKOB_from_list( tmp);
        }
        
        mutex_unlock(&kob_list_lock);
        
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

static ssize_t cryptochr_read(struct file *filp, char __user *ubuff, size_t count,loff_t *offp) {
        
        const char * ptr = readbuf + *offp;
		unsigned short n = 0;
        
        PDEBUG("Called read() function in cryptochr readbuf=%s count=%ld offs=%lld (@%p) \n", readbuf, count, *offp, offp);
        
        if (NULL != readbuf && *readbuf ) {
                while (count && *ptr) {
                    
                        put_user(*(ptr++), ubuff++);
                        count--;
                        n++;
                }
                
                *offp += n;
                return n;
        }
        else return 0;
}

static void encrypt_Actions( const char *msg, const char *key, const char *id) {
    
        struct kmessage *tmp_kmesg=NULL;
        PDEBUG("encrypt called");
        
        /* Creates a node, fills it and encrypts message*/
        tmp_kmesg = kmalloc(sizeof(struct kmessage), GFP_KERNEL);

        tmp_kmesg->message = kmalloc(MSG_BUFF_SIZE, GFP_KERNEL);
        memset( tmp_kmesg->message, 0, MSG_BUFF_SIZE);
        strncpy(tmp_kmesg->message, msg,MSG_BUFF_SIZE-1);

        tmp_kmesg->id = kmalloc(ID_BUFF_SIZE, GFP_KERNEL);
        memset(tmp_kmesg->id, 0, ID_BUFF_SIZE);
        strncpy(tmp_kmesg->id, id, ID_BUFF_SIZE - 1);

        encrypt(tmp_kmesg->message, key, MSG_BUFF_SIZE);
    
        /* Add the node to the tail of the list */
        list_add_tail(&tmp_kmesg->list, &msg_head);
        PDEBUG("Created kmessage, memory used: %ld\n",
            sizeof(tmp_kmesg) + sizeof(tmp_kmesg->message) + sizeof(tmp_kmesg->id));

}

static void set_timer(void) {
        
        ktime_t ktime;
        mutex_lock(&timer_lock);
        
        /* set timer */
        if(hr_timer_on) {
                hrtimer_cancel( &hr_timer);
        }
        
        ktime = ktime_set(0, MS_TO_NS(message_life_timeout));
        hrtimer_init(&hr_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
        hr_timer.function = &lock;
        hrtimer_start(&hr_timer, ktime, HRTIMER_MODE_REL);
        hr_timer_on = true;
        mutex_unlock(&timer_lock);
}

static void decript_Action(const char * key, const char *id) {
    
        struct kmessage *tmp_kmesg=NULL;
        unsigned int found = 0;
        struct list_head *next;
        
        PDEBUG("decrypt called");
        
        list_for_each_safe(pos, next, &msg_head) {
                tmp_kmesg = list_entry(pos, struct kmessage, list);
                if (strcmp(tmp_kmesg->id, id) == 0) {
                        found++;
                        break;
                }
        }

        if (found) {
            
            int retval;
            struct kobject *example_kobj= NULL;
            
            memcpy( readbuf, tmp_kmesg->message, MSG_BUFF_SIZE);
            encrypt(readbuf, key, MSG_BUFF_SIZE);

            set_timer();
            
            /* delete the message */
            list_del(&tmp_kmesg->list);
            kfree(tmp_kmesg->message);
            kfree(tmp_kmesg->id);
            kfree(tmp_kmesg);

            // TODO check if such kobject already exists
            example_kobj=kobject_create_and_add( id, cool_dev_kobj);
            
            if( example_kobj ) {
                    retval = sysfs_create_file(example_kobj, &o_attr.attr);
                    if (retval) {
                        kobject_put(example_kobj);
                    } 
                
                    else {
                        // create node inside new list with this id, message and timer
                        struct kobs * tmp_kob;
                        ktime_t ktime;
                        
                        tmp_kob = kmalloc(sizeof(struct kobs), GFP_KERNEL);
                        tmp_kob->message = kmalloc(MSG_BUFF_SIZE, GFP_KERNEL);
                        memcpy( tmp_kob->message, readbuf, MSG_BUFF_SIZE);
                        tmp_kob->self = example_kobj;

                        memset( &tmp_kob->timer, 0, sizeof(struct hrtimer));
                        hrtimer_init(&tmp_kob->timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
                        tmp_kob->timer.function = &del_ko;
                        ktime = ktime_set(0, MS_TO_NS(message_life_timeout));
                        hrtimer_start(&tmp_kob->timer, ktime, HRTIMER_MODE_REL);

                        mutex_lock(&kob_list_lock);
                        
                        list_add_tail(&tmp_kob->list, &kob_head);
                        
                        mutex_unlock(&kob_list_lock);
                    }
            }
        }
            else {
                PERR("No such ID: %s\n", id);
            }
}

static ssize_t cryptochr_write(struct file *filp, const char __user *ubuff, size_t count, loff_t *offp) {
        
        int res;
        enum _CMD CMD = CMD_NONE;
        unsigned short i = 0;
        char *message=NULL, *m = NULL, *m_orig;
        char *found=NULL, *command=NULL, *key=NULL, *msg=NULL, *id=NULL;
		
        PDEBUG("Called write() function in cryptochr\n");

		/* raw message from userspace */
		message = kmalloc(CRYPTOCHR_BUFF_SIZE, GFP_KERNEL);
        res = copy_from_user(message, ubuff, count);

		if (res) {
				PERR("Failed to copy from userspace\n");
				return -EFAULT;
		}
		
        message[count] = '\0';
        PINFO("Message passed: %s\n", message);
        
        /* Actually splits passed string */
        m_orig = m = kstrdup(message, GFP_KERNEL);
        
        while( (found = strsep(&m, " ")) != NULL ) {
                    i++;
					PDEBUG("i=%d, found=%s\n", i, found);
                    switch (i) {
                        /* encrypt key message id
                         * decrypt key id
                         */
                      case 1:
                            command = found;
                            if (is_decrypt(command))
                                    CMD = CMD_DECRYPT;
                            else if (is_encrypt(command))
                                    CMD = CMD_ENCRYPT;
                            else {
                                    PERR("Wrong command '%s'\n", command);
                                    return -EINVAL;}
                            break;
                            
                      case 2:
                            key = found;
                            break;
                            
                      case 3:
                            if( CMD_DECRYPT == CMD) {
                                    id = found;
                                if( id[strlen(id)-1] == '\n' || id[strlen(id)-1] == '\r') {
                                    id[strlen(id)-1] = '\0';
                                }
                            }
                            else if (CMD_ENCRYPT == CMD)
                                    msg = found;
                            break;
                            
                      case 4:
                            if ( CMD_ENCRYPT == CMD ) {
                                id = found;
                                if( id[strlen(id)-1] == '\n' || id[strlen(id)-1] == '\r') {
                                    id[strlen(id)-1] = '\0';
                                }
                            }
                            else {
                                PERR("Passed redundant parameter: %s", found);
                            }
                            break;
                            
                      default:
                            PERR("Passed redundant parameter: %s", found);
                    };
        }

        /* recognize command */
        if (CMD_ENCRYPT == CMD ) {
            
                if( i > 3) {
                        encrypt_Actions( msg, key, id);
                } 
                else {
                        PERR("ENCRYPT command issued with less then 4 parameters");
                }
        }
        
        else if (CMD_DECRYPT == CMD ) {
            
                if( i > 2 ) {
                    decript_Action( key, id);
                } else {
                    PERR("ENCRYPT command issued with less then 3 parameters");
                }
        }

    	kfree(m_orig);
		kfree(message);
        return count;
} 

static int chryptochr_dev_uevent(struct device *dev, struct kobj_uevent_env *env) {
        
        add_uevent_var(env, "DEVMODE=%#o", 0666);
        return 0;
}

static const struct file_operations cryptochr_fops = {
    
	    .owner = THIS_MODULE,
	    .open = cryptochr_open,
	    .release = cryptochr_release,
	    .read = cryptochr_read,
	    .write = cryptochr_write,
};
    
static int __init cryptochr_init(void) {
    
        int i=0;
        int res=0;

        res = alloc_chrdev_region(&cryptochr_d,CRYPTOCHR_FIRST_MINOR, CRYPTOCHR_N_MINORS ,DRIVER_NAME);
        
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
        
        hr_timer_on = false;
        mutex_init(&timer_lock);
        
        cryptochr_class->dev_uevent = chryptochr_dev_uevent;

        cryptochr_d = MKDEV(cryptochr_major ,CRYPTOCHR_FIRST_MINOR+i);

        cdev_init(&cryptochr_device.cdev , &cryptochr_fops);

        cdev_add(&cryptochr_device.cdev,cryptochr_d,1);

        cryptochr_device.cryptochr_device =
            device_create(cryptochr_class, NULL, cryptochr_d, NULL,
                CRYPTOCHR_NODE_NAME);

        if(!cryptochr_device.cryptochr_device) {

                class_destroy(cryptochr_class);
                PERR("device creation failed\n");
                return -1;
        }

        cool_dev_kobj = &cryptochr_device.cryptochr_device->kobj;
        
        /* Initialize the list with the head variable declared above */
        INIT_LIST_HEAD(&(msg_head));
        INIT_LIST_HEAD(&(kob_head));
        
        mutex_init(&kob_list_lock);
        
        /* Allocate the buffer */
        readbuf = kmalloc(MSG_BUFF_SIZE, GFP_KERNEL);
        memset( readbuf, 0, MSG_BUFF_SIZE);
        
        cryptochr_device.minor = CRYPTOCHR_FIRST_MINOR;

        PDEBUG("initialization completed\n");

        return 0;
}
    
static void __exit cryptochr_exit(void) {
    
        static struct kmessage *tmp_kmesg;
        struct list_head *next=NULL;
        struct kobs * tmp_kob;
        
        PDEBUG("EXIT\n");

        cryptochr_d = MKDEV(cryptochr_major, CRYPTOCHR_FIRST_MINOR);
        mutex_lock( &timer_lock);
        
        if( hr_timer_on ) {
                hrtimer_cancel( &hr_timer);
        }
        
        mutex_unlock( &timer_lock);

        /* Deallocate the buffer */
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
                kfree(tmp_kmesg->id);
                kfree(tmp_kmesg);
        }

        next = NULL;
        mutex_lock(&kob_list_lock);
        
        list_for_each_safe(pos, next, &kob_head) {
            
                tmp_kob = list_entry(pos, struct kobs, list);
                list_del(pos);

                hrtimer_cancel( &tmp_kob->timer);
                unlinkObj(tmp_kob);
                removeKOB_from_list( tmp_kob);
        }
        
        mutex_unlock(&kob_list_lock);
        
        cdev_del(&cryptochr_device.cdev);

        device_destroy(cryptochr_class ,cryptochr_d);

        class_destroy(cryptochr_class);

        unregister_chrdev_region(cryptochr_d ,CRYPTOCHR_N_MINORS);
}

module_init(cryptochr_init);

module_exit(cryptochr_exit);

module_param(message_life_timeout, ulong, S_IRUGO);

