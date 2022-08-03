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
#include <linux/interrupt.h>
#include <linux/of_irq.h>
#include <linux/wait.h>
#include <linux/ide.h>
#include <linux/poll.h>
#include <linux/platform_device.h>

#define  DTSPLATLED_COUNT           1
#define  DTSPLATLED_NAME            "dtsplatled"

#if 0
#define  NODE_NAME                  "/gpioled"
#endif
#define  PROP_NAME                  "led-gpio"

struct dtsplatled_dev{
    dev_t devid;
    int major;
    int minor;
    struct cdev cdev;
    struct class *pclass;
    struct device *pdevice;
    struct device_node *dn;
    int led_gpio;
};
struct dtsplatled_dev *dtsplatled;

int dtsplatled_open(struct inode *pinode, struct file *filp)
{
    filp->private_data = &dtsplatled->led_gpio;
    return 0;
}
ssize_t dtsplatled_write(struct file *filp, const char __user *pbuf, size_t count, loff_t *ploff)
{
    char data = 0;
    int ret = 0;
    int ledgpio = *(int *)filp->private_data;
    ret = copy_from_user(&data,pbuf,count);
    if(ret != 0)
    {
        printk("copy_from_user error!\n");
        return -EFAULT;
    }
    printk("kernel write count:%d!\n",count);

    if(data == 1)
        gpio_set_value(ledgpio,0);
    else if(data == 0)
        gpio_set_value(ledgpio,1);
    else
    {
        printk("command error!\n");
        return -EFAULT;
    }
    printk("dtsplatled_write success!\n");
    return 0;
}
int dtsplatled_close(struct inode *pinode, struct file *filp)
{
    return 0;
}

const struct file_operations dtsplatled_fops = {
    .owner = THIS_MODULE,
    .open = dtsplatled_open,
    .write = dtsplatled_write,
    .release = dtsplatled_close
};

int dtsplatleddrv_probe(struct platform_device *pdev)
{
    int ret = 0;
    dtsplatled = (struct dtsplatled_dev *)kmalloc(sizeof(struct dtsplatled_dev),GFP_KERNEL);
    if(!dtsplatled)
    {
        printk("kmalloc error!\n");
        return -EINVAL;
    }

    dtsplatled->major = 0;
    dtsplatled->minor = 0;
    if(dtsplatled->major)
    {
        dtsplatled->devid = MKDEV(dtsplatled->major,dtsplatled->minor);
        ret = register_chrdev_region(dtsplatled->devid,DTSPLATLED_COUNT,DTSPLATLED_NAME);
        if(ret < 0)
        {
            printk("register_chrdev_region error!\n");
            goto chrdev_region_error;
        }
    }else
    {
        ret = alloc_chrdev_region(&dtsplatled->devid,dtsplatled->minor,DTSPLATLED_COUNT,DTSPLATLED_NAME);
        if(ret < 0)
        {
            printk("alloc_chrdev_region error!\n");
            goto chrdev_region_error;
        }
        dtsplatled->major = MAJOR(dtsplatled->devid);
        dtsplatled->minor = MINOR(dtsplatled->devid);
    }
    printk("dtsplatled major:%d,minor:%d!\n",dtsplatled->major,dtsplatled->minor);
    dtsplatled->cdev.owner = THIS_MODULE;
    cdev_init(&dtsplatled->cdev,&dtsplatled_fops);
    ret = cdev_add(&dtsplatled->cdev,dtsplatled->devid,DTSPLATLED_COUNT);
    if(ret < 0)
    {
        printk("cdev_add error!\n");
        goto cdev_add_error;
    }
    dtsplatled->pclass = class_create(THIS_MODULE,DTSPLATLED_NAME);
    if(IS_ERR(dtsplatled->pclass))
    {
        printk("class_create error!\n");
        ret = PTR_ERR(dtsplatled->pclass);
        goto class_create_error;
    }
    dtsplatled->pdevice = device_create(dtsplatled->pclass,NULL,dtsplatled->devid,NULL,DTSPLATLED_NAME);
    if(IS_ERR(dtsplatled->pdevice))
    {
        printk("device_create error!\n");
        ret = PTR_ERR(dtsplatled->pdevice);
        goto device_create_error;
    }

    dtsplatled->dn = pdev->dev.of_node;
#if 0
    dtsplatled->dn = of_find_node_by_path(NODE_NAME);
    if(!dtsplatled->dn)
    {
        printk("of_find_node_by_path error!\n");
        ret = -EINVAL;
        goto find_node_error;
    }
#endif
    dtsplatled->led_gpio = of_get_named_gpio(dtsplatled->dn,PROP_NAME,0);
    if(dtsplatled->led_gpio < 0)
    {
        printk("of_get_named_gpio error!\n");
        ret = dtsplatled->led_gpio;
        goto get_gpio_error;
    }
    ret = gpio_request(dtsplatled->led_gpio,DTSPLATLED_NAME);
    if(ret != 0)
    {
        printk("gpio_request error!\n");
        goto gpio_request_error;
    }
    ret = gpio_direction_output(dtsplatled->led_gpio,1);
    if(ret != 0)
    {
        printk("gpio_direction_output error!\n");
        goto gpio_output_error;
    }
    gpio_set_value(dtsplatled->led_gpio,0);
    printk("dtsplatleddrv_probe success!\n");
    return 0;

    gpio_output_error:
        gpio_free(dtsplatled->led_gpio);
    gpio_request_error:
    get_gpio_error:
#if 0
    find_node_error:
#endif
        device_destroy(dtsplatled->pclass,dtsplatled->devid);
    device_create_error:
        class_destroy(dtsplatled->pclass);
    class_create_error:
        cdev_del(&dtsplatled->cdev);
    cdev_add_error:
        unregister_chrdev_region(dtsplatled->devid,DTSPLATLED_COUNT);
    chrdev_region_error:
        kfree(dtsplatled);
    return ret;
    return 0;
}
int dtsplatleddrv_remove(struct platform_device *pdev)
{
    gpio_set_value(dtsplatled->led_gpio,1);
    gpio_free(dtsplatled->led_gpio);
    device_destroy(dtsplatled->pclass,dtsplatled->devid);
    class_destroy(dtsplatled->pclass);
    cdev_del(&dtsplatled->cdev);
    unregister_chrdev_region(dtsplatled->devid,DTSPLATLED_COUNT);
    kfree(dtsplatled);
    printk("dtsplatleddrv_remove success!\n");
    return 0;
}

const struct of_device_id dtsplatleddrv_oftable[] = {
    {.compatible = "atkalpha-gpioled"},
    { /****************************/ }
};
struct platform_driver dtsplatleddrv = {
    .driver = {
        .name = "dtsplatled",
        .of_match_table = dtsplatleddrv_oftable
    },
    .probe = dtsplatleddrv_probe,
    .remove = dtsplatleddrv_remove
};

static int __init dtsplatled_init(void)
{
    return platform_driver_register(&dtsplatleddrv);
}
static void __exit dtsplatled_exit(void)
{
    platform_driver_unregister(&dtsplatleddrv);
}

module_init(dtsplatled_init);
module_exit(dtsplatled_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("dong");