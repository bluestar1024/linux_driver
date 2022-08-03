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

#define  TIMER_COUNT         1
#define  TIMER_NAME          "timer"

#define  NODE_NAME          "/gpioled"
#define  PROP_NAME          "led-gpio"

struct timer_dev{
    dev_t devid;
    int major;
    int minor;
    struct cdev cdev;
    struct class *pclass;
    struct device *pdevice;
    struct device_node *dn;
    int led_gpio;
    struct timer_list timer;
};
struct timer_dev *timer;

int timer_open(struct inode *pinode, struct file *filp)
{
    filp->private_data = timer;
    return 0;
}
ssize_t timer_write(struct file *filp, const char __user *pbuf, size_t count, loff_t *ploff)
{
    return 0;
}
ssize_t timer_read(struct file *filp, char __user *pbuf, size_t count, loff_t *ploff)
{
    return 0;
}
int timer_close(struct inode *pinode, struct file *filp)
{
    return 0;
}

const struct file_operations timer_fops = {
    .owner = THIS_MODULE,
    .open = timer_open,
    .write = timer_write,
    .read = timer_read,
    .release = timer_close
};

void timer_handle(unsigned long arg)
{
    static int status = 0;
    struct timer_dev *dev = (struct timer_dev *)arg;
    status = !status;
    //printk("status:%d!\n",status);
    gpio_set_value(dev->led_gpio,status);
    mod_timer(&dev->timer,jiffies + msecs_to_jiffies(500));
}

int ledio_init(void)
{
    int ret = 0;
    timer->dn = of_find_node_by_path(NODE_NAME);
    if(!timer->dn)
    {
        printk("of_find_node_by_path error!\n");
        return -EINVAL;
    }
    timer->led_gpio = of_get_named_gpio(timer->dn,PROP_NAME,0);
    if(timer->led_gpio < 0)
    {
        printk("of_get_named_gpio error!\n");
        ret = timer->led_gpio;
        goto get_gpio_error;
    }
    printk("led gpio:%d!\n",timer->led_gpio);
    ret = gpio_request(timer->led_gpio,TIMER_NAME);
    if(ret != 0)
    {
        printk("gpio_request error!\n");
        goto gpio_request_error;
    }
    ret = gpio_direction_output(timer->led_gpio,1);
    if(ret != 0)
    {
        printk("gpio_direction_output error!\n");
        goto gpio_output_error;
    }
    gpio_set_value(timer->led_gpio,0);
    return 0;

    gpio_output_error:
        gpio_free(timer->led_gpio);
    gpio_request_error:
    get_gpio_error:
    return ret;
}

void timer_startup_init(void)
{
    init_timer(&timer->timer);
    timer->timer.expires = jiffies + msecs_to_jiffies(500);
    timer->timer.function = timer_handle;
    timer->timer.data = (unsigned long)timer;
    add_timer(&timer->timer);
}

static int __init timer_init(void)
{
    int ret = 0;
    timer = (struct timer_dev *)kmalloc(sizeof(struct timer_dev),GFP_KERNEL);
    if(!timer)
    {
        printk("kmalloc error!\n");
        return -EINVAL;
    }

    timer->major = 0;
    timer->minor = 0;
    if(timer->major)
    {
        timer->devid = MKDEV(timer->major,timer->minor);
        ret = register_chrdev_region(timer->devid,TIMER_COUNT,TIMER_NAME);
        if(ret < 0)
        {
            printk("register_chrdev_region error!\n");
            goto chrdev_region_error;
        }
    }else
    {
        ret = alloc_chrdev_region(&timer->devid,timer->minor,TIMER_COUNT,TIMER_NAME);
        if(ret < 0)
        {
            printk("alloc_chrdev_region error!\n");
            goto chrdev_region_error;
        }
        timer->major = MAJOR(timer->devid);
        timer->minor = MINOR(timer->devid);
    }
    printk("timer major:%d,minor:%d!\n",timer->major,timer->minor);
    timer->cdev.owner = THIS_MODULE;
    cdev_init(&timer->cdev,&timer_fops);
    ret = cdev_add(&timer->cdev,timer->devid,TIMER_COUNT);
    if(ret < 0)
    {
        printk("cdev_add error!\n");
        goto cdev_add_error;
    }
    timer->pclass = class_create(THIS_MODULE,TIMER_NAME);
    if(IS_ERR(timer->pclass))
    {
        printk("class_create error!\n");
        ret = PTR_ERR(timer->pclass);
        goto class_create_error;
    }
    timer->pdevice = device_create(timer->pclass,NULL,timer->devid,NULL,TIMER_NAME);
    if(IS_ERR(timer->pdevice))
    {
        printk("device_create error!\n");
        ret = PTR_ERR(timer->pdevice);
        goto device_create_error;
    }

    ret = ledio_init();
    if(ret != 0)
    {
        printk("ledio_init error!\n");
        goto ledio_init_error;
    }

    timer_startup_init();
    printk("timer_init success!\n");
    return 0;

    ledio_init_error:
        device_destroy(timer->pclass,timer->devid);
    device_create_error:
        class_destroy(timer->pclass);
    class_create_error:
        cdev_del(&timer->cdev);
    cdev_add_error:
        unregister_chrdev_region(timer->devid,TIMER_COUNT);
    chrdev_region_error:
        kfree(timer);
    return ret;
}
static void __exit timer_exit(void)
{
    del_timer_sync(&timer->timer);
    gpio_set_value(timer->led_gpio,1);
    gpio_free(timer->led_gpio);
    device_destroy(timer->pclass,timer->devid);
    class_destroy(timer->pclass);
    cdev_del(&timer->cdev);
    unregister_chrdev_region(timer->devid,TIMER_COUNT);
    kfree(timer);
    printk("timer_exit success!\n");
}

module_init(timer_init);
module_exit(timer_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("dong");