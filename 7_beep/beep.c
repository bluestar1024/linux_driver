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

#define  BEEP_COUNT         1
#define  BEEP_NAME          "beep"

#define  NODE_NAME          "/beep"
#define  PROP_NAME          "beep-gpio"

struct beep_dev{
    dev_t devid;
    int major;
    int minor;
    struct cdev cdev;
    struct class *pclass;
    struct device *pdevice;
    struct device_node *dn;
    int beep_gpio;
};
struct beep_dev *beep;

int beep_open(struct inode *pinode, struct file *filp)
{
    filp->private_data = &beep->beep_gpio;
    return 0;
}
ssize_t beep_write(struct file *filp, const char __user *pbuf, size_t count, loff_t *ploff)
{
    char data = 0;
    int ret = 0;
    int beepgpio = *(int *)filp->private_data;
    ret = copy_from_user(&data,pbuf,count);
    if(ret != 0)
    {
        printk("copy_from_user error!\n");
        return -EFAULT;
    }
    printk("kernel write count:%d!\n",count);

    if(data == 1)
        gpio_set_value(beepgpio,0);
    else if(data == 0)
        gpio_set_value(beepgpio,1);
    else
    {
        printk("command error!\n");
        return -EFAULT;
    }
    printk("newchrled_write success!\n");
    return 0;
}
int beep_close(struct inode *pinode, struct file *filp)
{
    return 0;
}

const struct file_operations beep_fops = {
    .owner = THIS_MODULE,
    .open = beep_open,
    .write = beep_write,
    .release = beep_close
};

static int __init beep_init(void)
{
    int ret = 0;
    beep = (struct beep_dev *)kmalloc(sizeof(struct beep_dev),GFP_KERNEL);
    if(!beep)
    {
        printk("kmalloc error!\n");
        return -EINVAL;
    }

    beep->major = 0;
    beep->minor = 0;
    if(beep->major)
    {
        beep->devid = MKDEV(beep->major,beep->minor);
        ret = register_chrdev_region(beep->devid,BEEP_COUNT,BEEP_NAME);
        if(ret < 0)
        {
            printk("register_chrdev_region error!\n");
            goto chrdev_region_error;
        }
    }else
    {
        ret = alloc_chrdev_region(&beep->devid,beep->minor,BEEP_COUNT,BEEP_NAME);
        if(ret < 0)
        {
            printk("alloc_chrdev_region error!\n");
            goto chrdev_region_error;
        }
        beep->major = MAJOR(beep->devid);
        beep->minor = MINOR(beep->devid);
    }
    printk("beep major:%d,minor:%d!\n",beep->major,beep->minor);
    beep->cdev.owner = THIS_MODULE;
    cdev_init(&beep->cdev,&beep_fops);
    ret = cdev_add(&beep->cdev,beep->devid,BEEP_COUNT);
    if(ret < 0)
    {
        printk("cdev_add error!\n");
        goto cdev_add_error;
    }
    beep->pclass = class_create(THIS_MODULE,BEEP_NAME);
    if(IS_ERR(beep->pclass))
    {
        printk("class_create error!\n");
        ret = PTR_ERR(beep->pclass);
        goto class_create_error;
    }
    beep->pdevice = device_create(beep->pclass,NULL,beep->devid,NULL,BEEP_NAME);
    if(IS_ERR(beep->pdevice))
    {
        printk("device_create error!\n");
        ret = PTR_ERR(beep->pdevice);
        goto device_create_error;
    }

    beep->dn = of_find_node_by_path(NODE_NAME);
    if(!beep->dn)
    {
        printk("of_find_node_by_path error!\n");
        ret = -EINVAL;
        goto find_node_error;
    }
    beep->beep_gpio = of_get_named_gpio(beep->dn,PROP_NAME,0);
    if(beep->beep_gpio < 0)
    {
        printk("of_get_named_gpio error!\n");
        ret = beep->beep_gpio;
        goto get_gpio_error;
    }
    printk("beep gpio:%d!\n",beep->beep_gpio);
    ret = gpio_request(beep->beep_gpio,BEEP_NAME);
    if(ret != 0)
    {
        printk("gpio_request error!\n");
        goto gpio_request_error;
    }
    ret = gpio_direction_output(beep->beep_gpio,1);
    if(ret != 0)
    {
        printk("gpio_direction_output error!\n");
        goto gpio_output_error;
    }
    gpio_set_value(beep->beep_gpio,0);
    printk("beep_init success!\n");
    return 0;

    gpio_output_error:
        gpio_free(beep->beep_gpio);
    gpio_request_error:
    get_gpio_error:
    find_node_error:
        device_destroy(beep->pclass,beep->devid);
    device_create_error:
        class_destroy(beep->pclass);
    class_create_error:
        cdev_del(&beep->cdev);
    cdev_add_error:
        unregister_chrdev_region(beep->devid,BEEP_COUNT);
    chrdev_region_error:
        kfree(beep);
    return ret;
}
static void __exit beep_exit(void)
{
    gpio_set_value(beep->beep_gpio,1);
    gpio_free(beep->beep_gpio);
    device_destroy(beep->pclass,beep->devid);
    class_destroy(beep->pclass);
    cdev_del(&beep->cdev);
    unregister_chrdev_region(beep->devid,BEEP_COUNT);
    kfree(beep);
    printk("beep_exit success!\n");
}

module_init(beep_init);
module_exit(beep_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("dong");