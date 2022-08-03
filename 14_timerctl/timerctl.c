#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/io.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/of.h>
#include <linux/slab.h>
#include <linux/of_address.h>
#include <linux/of_gpio.h>
#include <linux/gpio.h>
#include <linux/timer.h>

#define  TIMERCTL_COUNT         1
#define  TIMERCTL_NAME          "timerctl"

#define  NODE_NAME              "/gpioled"
#define  PROP_NAME              "led-gpio"

#define  CLOSE_CMD              _IO(0xef,1)
#define  OPEN_CMD               _IO(0xef,2)
#define  SETPERIOD_CMD          _IOW(0xef,3,int)

struct timerctl_dev{
    dev_t devid;
    int major;
    int minor;
    struct cdev cdev;
    struct class *pclass;
    struct device *pdevice;
    struct device_node *dn;
    int led_gpio;
    struct timer_list timer;
    atomic_t timeperiod;
};
struct timerctl_dev *timerctl;

int timerctl_open(struct inode *pinode, struct file *filp)
{
    filp->private_data = timerctl;
    return 0;
}
long timerctl_ioctl(struct file *filp, unsigned int cmd, unsigned long data)
{
    struct timerctl_dev *dev = (struct timerctl_dev *)filp->private_data;
    switch(cmd)
    {
        case CLOSE_CMD:
            del_timer_sync(&dev->timer);
            break;
        case OPEN_CMD:
            mod_timer(&dev->timer,jiffies + msecs_to_jiffies(atomic_read(&timerctl->timeperiod)));
            break;
        case SETPERIOD_CMD:
            atomic_set(&dev->timeperiod,data);
            mod_timer(&dev->timer,jiffies + msecs_to_jiffies(atomic_read(&timerctl->timeperiod)));
            break;
    }
    return 0;
}
int timerctl_close(struct inode *pinode, struct file *filp)
{
    return 0;
}

const struct file_operations timerctl_fops = {
    .owner = THIS_MODULE,
    .open = timerctl_open,
    .unlocked_ioctl = timerctl_ioctl,
    .release = timerctl_close
};

void timer_handle(unsigned long arg)
{
    static int status = 0;
    struct timerctl_dev *dev = (struct timerctl_dev *)arg;
    status = !status;
    gpio_set_value(dev->led_gpio,status);
    mod_timer(&dev->timer,jiffies + msecs_to_jiffies(atomic_read(&timerctl->timeperiod)));
}

int ledio_init(void)
{
    int ret = 0;
    timerctl->dn = of_find_node_by_path(NODE_NAME);
    if(!timerctl->dn)
    {
        printk("of_find_node_by_path error!\n");
        return -EINVAL;
    }
    timerctl->led_gpio = of_get_named_gpio(timerctl->dn,PROP_NAME,0);
    if(timerctl->led_gpio < 0)
    {
        printk("of_get_named_gpio error!\n");
        ret = timerctl->led_gpio;
        goto get_gpio_error;
    }
    printk("led gpio:%d!\n",timerctl->led_gpio);
    ret = gpio_request(timerctl->led_gpio,TIMERCTL_NAME);
    if(ret != 0)
    {
        printk("gpio_request error!\n");
        goto gpio_request_error;
    }
    ret = gpio_direction_output(timerctl->led_gpio,1);
    if(ret != 0)
    {
        printk("gpio_direction_output error!\n");
        goto gpio_output_error;
    }
    gpio_set_value(timerctl->led_gpio,0);
    return 0;

    gpio_output_error:
        gpio_free(timerctl->led_gpio);
    gpio_request_error:
    get_gpio_error:
    return ret;
}

void timer_startup_init(void)
{
    init_timer(&timerctl->timer);
    timerctl->timer.expires = jiffies + msecs_to_jiffies(atomic_read(&timerctl->timeperiod));
    timerctl->timer.function = timer_handle;
    timerctl->timer.data = (unsigned long)timerctl;
    add_timer(&timerctl->timer);
}

static int __init timerctl_init(void)
{
    int ret = 0;
    timerctl = (struct timerctl_dev *)kmalloc(sizeof(struct timerctl_dev),GFP_KERNEL);
    if(!timerctl)
    {
        printk("kmalloc error!\n");
        return -EINVAL;
    }
    atomic_set(&timerctl->timeperiod,500);

    timerctl->major = 0;
    timerctl->minor = 0;
    if(timerctl->major)
    {
        timerctl->devid = MKDEV(timerctl->major,timerctl->minor);
        ret = register_chrdev_region(timerctl->devid,TIMERCTL_COUNT,TIMERCTL_NAME);
        if(ret < 0)
        {
            printk("register_chrdev_region error!\n");
            goto chrdev_region_error;
        }
    }else
    {
        ret = alloc_chrdev_region(&timerctl->devid,timerctl->minor,TIMERCTL_COUNT,TIMERCTL_NAME);
        if(ret < 0)
        {
            printk("alloc_chrdev_region error!\n");
            goto chrdev_region_error;
        }
        timerctl->major = MAJOR(timerctl->devid);
        timerctl->minor = MINOR(timerctl->devid);
    }
    printk("timerctl major:%d,minor:%d!\n",timerctl->major,timerctl->minor);
    timerctl->cdev.owner = THIS_MODULE;
    cdev_init(&timerctl->cdev,&timerctl_fops);
    ret = cdev_add(&timerctl->cdev,timerctl->devid,TIMERCTL_COUNT);
    if(ret < 0)
    {
        printk("cdev_add error!\n");
        goto cdev_add_error;
    }
    timerctl->pclass = class_create(THIS_MODULE,TIMERCTL_NAME);
    if(IS_ERR(timerctl->pclass))
    {
        printk("class_create error!\n");
        ret = PTR_ERR(timerctl->pclass);
        goto class_create_error;
    }
    timerctl->pdevice = device_create(timerctl->pclass,NULL,timerctl->devid,NULL,TIMERCTL_NAME);
    if(IS_ERR(timerctl->pdevice))
    {
        printk("device_create error!\n");
        ret = PTR_ERR(timerctl->pdevice);
        goto device_create_error;
    }

    ret = ledio_init();
    if(ret != 0)
    {
        printk("ledio_init error!\n");
        goto ledio_init_error;
    }

    timer_startup_init();
    printk("timerctl_init success!\n");
    return 0;

    ledio_init_error:
        device_destroy(timerctl->pclass,timerctl->devid);
    device_create_error:
        class_destroy(timerctl->pclass);
    class_create_error:
        cdev_del(&timerctl->cdev);
    cdev_add_error:
        unregister_chrdev_region(timerctl->devid,TIMERCTL_COUNT);
    chrdev_region_error:
        kfree(timerctl);
    return ret;
}
static void __exit timerctl_exit(void)
{
    del_timer_sync(&timerctl->timer);
    gpio_set_value(timerctl->led_gpio,1);
    gpio_free(timerctl->led_gpio);
    device_destroy(timerctl->pclass,timerctl->devid);
    class_destroy(timerctl->pclass);
    cdev_del(&timerctl->cdev);
    unregister_chrdev_region(timerctl->devid,TIMERCTL_COUNT);
    kfree(timerctl);
    printk("timerctl_exit success!\n");
}

module_init(timerctl_init);
module_exit(timerctl_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("dong");