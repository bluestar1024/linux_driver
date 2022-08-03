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
#include <linux/atomic.h>
#include <linux/spinlock.h>

#define  GPIOLED_COUNT      1
#define  GPIOLED_NAME       "gpioled"

#define  NODE_NAME          "/gpioled"
#define  PROP_NAME          "led-gpio"

struct gpioled_dev{
    dev_t devid;
    int major;
    int minor;
    struct cdev cdev;
    struct class *pclass;
    struct device *pdevice;
    struct device_node *dn;
    int led_gpio;

    int dev_status;
    spinlock_t lock;
    unsigned long irqflag;
};
struct gpioled_dev *gpioled;

int gpioled_open(struct inode *pinode, struct file *filp)
{
    filp->private_data = gpioled;
    //spin_lock(&gpioled->lock);
    spin_lock_irqsave(&gpioled->lock,gpioled->irqflag);
    if(gpioled->dev_status)
    {
        //spin_unlock(&gpioled->lock);
        spin_unlock_irqrestore(&gpioled->lock,gpioled->irqflag);
        return -EBUSY;
    }
    gpioled->dev_status++;
    //spin_unlock(&gpioled->lock);
    spin_unlock_irqrestore(&gpioled->lock,gpioled->irqflag);
    return 0;
}
ssize_t gpioled_write(struct file *filp, const char __user *pbuf, size_t count, loff_t *ploff)
{
    char data = 0;
    int ret = 0;
    struct gpioled_dev *dev = (struct gpioled_dev *)filp->private_data;
    ret = copy_from_user(&data,pbuf,count);
    if(ret != 0)
    {
        printk("copy_from_user error!\n");
        return -EFAULT;
    }
    printk("kernel write count:%d!\n",count);

    if(data == 1)
        gpio_set_value(dev->led_gpio,0);
    else if(data == 0)
        gpio_set_value(dev->led_gpio,1);
    else
    {
        printk("command error!\n");
        return -EFAULT;
    }
    printk("newchrled_write success!\n");
    return 0;
}
int gpioled_close(struct inode *pinode, struct file *filp)
{
    struct gpioled_dev *dev = (struct gpioled_dev *)filp->private_data;
    //spin_lock(&dev->lock);
    spin_lock_irqsave(&dev->lock,dev->irqflag);
    if(dev->dev_status)
        dev->dev_status--;
    //spin_unlock(&dev->lock);
    spin_unlock_irqrestore(&dev->lock,dev->irqflag);
    return 0;
}

const struct file_operations gpioled_fops = {
    .owner = THIS_MODULE,
    .open = gpioled_open,
    .write = gpioled_write,
    .release = gpioled_close
};

static int __init gpioled_init(void)
{
    int ret = 0;
    gpioled = (struct gpioled_dev *)kmalloc(sizeof(struct gpioled_dev),GFP_KERNEL);
    if(!gpioled)
    {
        printk("kmalloc error!\n");
        return -EINVAL;
    }
    spin_lock_init(&gpioled->lock);
    gpioled->dev_status = 0;

    gpioled->major = 0;
    gpioled->minor = 0;
    if(gpioled->major)
    {
        gpioled->devid = MKDEV(gpioled->major,gpioled->minor);
        ret = register_chrdev_region(gpioled->devid,GPIOLED_COUNT,GPIOLED_NAME);
        if(ret < 0)
        {
            printk("register_chrdev_region error!\n");
            goto chrdev_region_error;
        }
    }else
    {
        ret = alloc_chrdev_region(&gpioled->devid,gpioled->minor,GPIOLED_COUNT,GPIOLED_NAME);
        if(ret < 0)
        {
            printk("alloc_chrdev_region error!\n");
            goto chrdev_region_error;
        }
        gpioled->major = MAJOR(gpioled->devid);
        gpioled->minor = MINOR(gpioled->devid);
    }
    printk("gpioled major:%d,minor:%d!\n",gpioled->major,gpioled->minor);
    gpioled->cdev.owner = THIS_MODULE;
    cdev_init(&gpioled->cdev,&gpioled_fops);
    ret = cdev_add(&gpioled->cdev,gpioled->devid,GPIOLED_COUNT);
    if(ret < 0)
    {
        printk("cdev_add error!\n");
        goto cdev_add_error;
    }
    gpioled->pclass = class_create(THIS_MODULE,GPIOLED_NAME);
    if(IS_ERR(gpioled->pclass))
    {
        printk("class_create error!\n");
        ret = PTR_ERR(gpioled->pclass);
        goto class_create_error;
    }
    gpioled->pdevice = device_create(gpioled->pclass,NULL,gpioled->devid,NULL,GPIOLED_NAME);
    if(IS_ERR(gpioled->pdevice))
    {
        printk("device_create error!\n");
        ret = PTR_ERR(gpioled->pdevice);
        goto device_create_error;
    }

    gpioled->dn = of_find_node_by_path(NODE_NAME);
    if(!gpioled->dn)
    {
        printk("of_find_node_by_path error!\n");
        ret = -EINVAL;
        goto find_node_error;
    }
    gpioled->led_gpio = of_get_named_gpio(gpioled->dn,PROP_NAME,0);
    if(gpioled->led_gpio < 0)
    {
        printk("of_get_named_gpio error!\n");
        ret = gpioled->led_gpio;
        goto get_gpio_error;
    }
    ret = gpio_request(gpioled->led_gpio,GPIOLED_NAME);
    if(ret != 0)
    {
        printk("gpio_request error!\n");
        goto gpio_request_error;
    }
    ret = gpio_direction_output(gpioled->led_gpio,1);
    if(ret != 0)
    {
        printk("gpio_direction_output error!\n");
        goto gpio_output_error;
    }
    gpio_set_value(gpioled->led_gpio,0);
    printk("gpioled_init success!\n");
    return 0;

    gpio_output_error:
        gpio_free(gpioled->led_gpio);
    gpio_request_error:
    get_gpio_error:
    find_node_error:
        device_destroy(gpioled->pclass,gpioled->devid);
    device_create_error:
        class_destroy(gpioled->pclass);
    class_create_error:
        cdev_del(&gpioled->cdev);
    cdev_add_error:
        unregister_chrdev_region(gpioled->devid,GPIOLED_COUNT);
    chrdev_region_error:
        kfree(gpioled);
    return ret;
}
static void __exit gpioled_exit(void)
{
    gpio_set_value(gpioled->led_gpio,1);
    gpio_free(gpioled->led_gpio);
    device_destroy(gpioled->pclass,gpioled->devid);
    class_destroy(gpioled->pclass);
    cdev_del(&gpioled->cdev);
    unregister_chrdev_region(gpioled->devid,GPIOLED_COUNT);
    kfree(gpioled);
    printk("gpioled_exit success!\n");
}

module_init(gpioled_init);
module_exit(gpioled_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("dong");