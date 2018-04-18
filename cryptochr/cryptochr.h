
#define DRIVER_NAME "cryptochr"
#define PDEBUG(fmt,args...) printk(KERN_DEBUG"%s:"fmt,DRIVER_NAME, ##args)
#define PERR(fmt,args...) printk(KERN_ERR"%s:"fmt,DRIVER_NAME,##args)
#define PINFO(fmt,args...) printk(KERN_INFO"%s:"fmt,DRIVER_NAME, ##args)

#include<linux/cdev.h>
#include<linux/circ_buf.h>
#include<linux/device.h>
#include<linux/fs.h>
#include<linux/init.h>
#include<linux/interrupt.h>
#include<linux/kdev_t.h>
#include<linux/module.h>
#include<linux/moduleparam.h>
#include<linux/sched.h>
#include<linux/semaphore.h>
#include<linux/slab.h>
#include<linux/types.h>
#include<linux/uaccess.h>
#include<linux/wait.h>
#include <linux/timer.h>
#include <linux/time.h>
#include <linux/jiffies.h>
#include <linux/delay.h>
#include <linux/kernel.h>
#include <linux/hrtimer.h>
#include <linux/ktime.h>
#include <linux/moduleloader.h>
#include <linux/list.h>
#include <linux/list_sort.h>
#include <linux/mutex.h>
#include <linux/sort.h>
