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
#include <linux/miscdevice.h>

#define  MISCBEEP_NAME          "miscbeep"
#define  MISCBEEP_MINOR         255
#define  PROP_NAME              "beep-gpio"
#define  GPIO_NAME              "beep_gpio"

struct miscbeep_dev{
    struct device_node *dn;
    int beep_gpio;
};
struct miscbeep_dev *miscbeep;

int miscbeep_open(struct inode *pinode, struct file *filp)
{
    filp->private_data = miscbeep;
    return 0;
}
ssize_t miscbeep_write(struct file *filp, const char __user *pbuf, size_t count, loff_t *ploff)
{
    char data = 0;
    int ret = 0;
    struct miscbeep_dev *dev = (struct miscbeep_dev *)filp->private_data;
    ret = copy_from_user(&data,pbuf,count);
    if(ret != 0)
    {
        printk("copy_from_user error!\n");
        return -EFAULT;
    }
    printk("kernel write count:%d!\n",count);

    if(data == 1)
        gpio_set_value(dev->beep_gpio,0);
    else if(data == 0)
        gpio_set_value(dev->beep_gpio,1);
    else
    {
        printk("command error!\n");
        return -EFAULT;
    }
    return 0;
}
int miscbeep_close(struct inode *pinode, struct file *filp)
{
    return 0;
}

const struct file_operations miscbeep_fops = {
    .owner = THIS_MODULE,
    .open = miscbeep_open,
    .write = miscbeep_write,
    .release = miscbeep_close
};
struct miscdevice miscbeep_mdev = {
    .minor = MISCBEEP_MINOR,
    .name = MISCBEEP_NAME,
    .fops = &miscbeep_fops
};

int miscbeepdrv_probe(struct platform_device *pdev)
{
    int ret = 0;
    miscbeep = (struct miscbeep_dev *)kmalloc(sizeof(struct miscbeep_dev),GFP_KERNEL);
    if(!miscbeep)
    {
        printk("kmalloc error!\n");
        return -EINVAL;
    }

    miscbeep->dn = pdev->dev.of_node;
    miscbeep->beep_gpio = of_get_named_gpio(miscbeep->dn,PROP_NAME,0);
    if(miscbeep->beep_gpio < 0)
    {
        printk("of_get_named_gpio error!\n");
        ret = miscbeep->beep_gpio;
        goto get_gpio_error;
    }
    ret = gpio_request(miscbeep->beep_gpio,GPIO_NAME);
    if(ret)
    {
        printk("gpio_request error!\n");
        goto gpio_request_error;
    }
    ret = gpio_direction_output(miscbeep->beep_gpio,0);
    if(ret)
    {
        printk("gpio_direction_output error!\n");
        goto gpio_output_error;
    }

    ret = misc_register(&miscbeep_mdev);
    if(ret)
    {
        printk("misc_register error!\n");
        goto register_misc_error;
    }
    printk("miscbeepdrv_probe success!\n");
    return 0;

    register_misc_error:
    gpio_output_error:
        gpio_free(miscbeep->beep_gpio);
    gpio_request_error:
    get_gpio_error:
        kfree(miscbeep);
    return ret;
}
int miscbeepdrv_remove(struct platform_device *pdev)
{
    gpio_set_value(miscbeep->beep_gpio,1);
    misc_deregister(&miscbeep_mdev);
    gpio_free(miscbeep->beep_gpio);
    kfree(miscbeep);
    printk("miscbeepdrv_remove success!\n");
    return 0;
}

const struct of_device_id miscbeepdrv_oftable[] = {
    {.compatible = "atkalpha-beep"},
    { /*************************/ }
};
struct platform_driver miscbeepdrv = {
    .driver = {
        .name = "miscbeepdrv",
        .of_match_table = miscbeepdrv_oftable
    },
    .probe = miscbeepdrv_probe,
    .remove = miscbeepdrv_remove
};

static int __init miscbeep_init(void)
{
    return platform_driver_register(&miscbeepdrv);
}
static void __exit miscbeep_exit(void)
{
    platform_driver_unregister(&miscbeepdrv);
}

module_init(miscbeep_init);
module_exit(miscbeep_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("dong");