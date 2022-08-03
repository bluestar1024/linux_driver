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
#include <linux/input.h>
#include <linux/err.h>
#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/spi/spi.h>
#include <linux/input/mt.h>
#include <linux/regmap.h>
#include "ap3216creg.h"

#define  I2CREGMAP_CNT               1
#define  I2CREGMAP_NAME              "i2cregmap"

struct i2cregmap_dev{
    dev_t devid;
    int major;
    int minor;
    struct cdev cdev;
    struct class *class;
    struct device *device;
    struct regmap *regmap;
    struct regmap_config regmap_config;

    u16 ir,als,ps;
};
struct i2cregmap_dev *i2cregmap;

u8 ap3216c_read_reg(struct i2cregmap_dev *dev, u8 reg)
{
    u32 data = 0;
    regmap_read(dev->regmap,reg,&data);
    return (u8)data;
}
void ap3216c_write_reg(struct i2cregmap_dev *dev, u8 reg, u8 value)
{
    regmap_write(dev->regmap,reg,value);
}
void ap3216c_read_regs(struct i2cregmap_dev *dev, u8 reg, u8 *buf, u8 len)
{
    regmap_bulk_read(dev->regmap,reg,buf,len);
}

void ap3216c_read_data(struct i2cregmap_dev *dev)
{
    u8 buf[6];
#if 1
    int i = 0;
    for(;i<6;i++)
        buf[i] = ap3216c_read_reg(dev,IR_DATA_LOW+i);
#endif
#if 0
    ap3216c_read_regs(dev,IR_DATA_LOW,buf,6);   //error!
#endif

    if(buf[0]&0x80)
        dev->ir = 0;
    else
        dev->ir = ((u16)buf[1] << 2) | (buf[0] & 0x03);

    dev->als = ((u16)buf[3] << 8) | buf[2];

    if(buf[4] & 0x40)
        dev->ps = 0;
    else
        dev->ps = ((u16)(buf[5] & 0x3F) << 4) | (buf[4] & 0x0F);
}

int i2cregmap_open(struct inode * inode, struct file *filp)
{
    u8 value = 0;
    filp->private_data = i2cregmap;
    ap3216c_write_reg(i2cregmap,SYSTEM_CONFIGURATION,0x4);
    mdelay(50);
    ap3216c_write_reg(i2cregmap,SYSTEM_CONFIGURATION,0x3);
    value = ap3216c_read_reg(i2cregmap,SYSTEM_CONFIGURATION);
    printk("system_config reg = %#x!\n",value);
    return 0;
}
ssize_t i2cregmap_read(struct file *filp, char __user *buf, size_t size, loff_t *loff)
{
    struct i2cregmap_dev *dev = (struct i2cregmap_dev *)filp->private_data;
    u16 data[3];
    int ret = 0;
    ap3216c_read_data(dev);
    data[0] = dev->ir;
    data[1] = dev->als;
    data[2] = dev->ps;
    ret = copy_to_user(buf,data,size);
    if(ret < 0)
    {
        printk("copy_to_user error!\n");
        return ret;
    }
    return 0;
}
int i2cregmap_close(struct inode *inode, struct file *filp)
{
    return 0;
}

const struct file_operations i2cregmap_fops = {
    .owner = THIS_MODULE,
    .open = i2cregmap_open,
    .read = i2cregmap_read,
    .release = i2cregmap_close
};

int	i2cregmap_probe(struct i2c_client *i2c, const struct i2c_device_id *id_table)
{
    int ret = 0;
    i2cregmap = (struct i2cregmap_dev *)devm_kzalloc(&i2c->dev,sizeof(struct i2cregmap_dev),GFP_KERNEL);
    if(IS_ERR(i2cregmap))
    {
        printk("devm_kzalloc error!\n");
        ret = PTR_ERR(i2cregmap);
        return ret;
    }

    i2cregmap->major = 0;
    i2cregmap->minor = 0;
    if(i2cregmap->major)
    {
        i2cregmap->devid = MKDEV(i2cregmap->major,i2cregmap->minor);
        ret = register_chrdev_region(i2cregmap->devid,I2CREGMAP_CNT,I2CREGMAP_NAME);
        if(ret < 0)
        {
            printk("register_chrdev_region error!\n");
            goto register_chrdev_error;
        }
    }
    else
    {
        ret = alloc_chrdev_region(&i2cregmap->devid,i2cregmap->minor,I2CREGMAP_CNT,I2CREGMAP_NAME);
        if(ret < 0)
        {
            printk("alloc_chrdev_region error!\n");
            goto register_chrdev_error;
        }
        i2cregmap->major = MAJOR(i2cregmap->devid);
        i2cregmap->minor = MINOR(i2cregmap->devid);
    }

    i2cregmap->cdev.owner = THIS_MODULE;
    cdev_init(&i2cregmap->cdev,&i2cregmap_fops);
    ret = cdev_add(&i2cregmap->cdev,i2cregmap->devid,I2CREGMAP_CNT);
    if(ret < 0)
    {
        printk("cdev_add error!\n");
        goto add_cdev_error;
    }

    i2cregmap->class = class_create(THIS_MODULE,I2CREGMAP_NAME);
    if(IS_ERR(i2cregmap->class))
    {
        printk("class_create error!\n");
        ret = PTR_ERR(i2cregmap->class);
        goto create_class_error;
    }
    i2cregmap->device = device_create(i2cregmap->class,NULL,i2cregmap->devid,NULL,I2CREGMAP_NAME);
    if(IS_ERR(i2cregmap->device))
    {
        printk("device_create error!\n");
        ret = PTR_ERR(i2cregmap->device);
        goto create_device_error;
    }

    i2cregmap->regmap_config.reg_bits = 8;
    i2cregmap->regmap_config.val_bits = 8;

    i2cregmap->regmap = regmap_init_i2c(i2c,&i2cregmap->regmap_config);
    if(IS_ERR(i2cregmap->regmap))
    {
        printk("regmap_init_spi error!\n");
        ret = PTR_ERR(i2cregmap->regmap);
        goto regmap_init_spi_error;
    }
    printk("i2cregmap_probe success!\n");
    return 0;

    regmap_init_spi_error:
        device_destroy(i2cregmap->class,i2cregmap->devid);
    create_device_error:
        class_destroy(i2cregmap->class);
    create_class_error:
        cdev_del(&i2cregmap->cdev);
    add_cdev_error:
        unregister_chrdev_region(i2cregmap->devid,I2CREGMAP_CNT);
    register_chrdev_error:
    return ret;
}
int	i2cregmap_remove(struct i2c_client *i2c)
{
    regmap_exit(i2cregmap->regmap);
    device_destroy(i2cregmap->class,i2cregmap->devid);
    class_destroy(i2cregmap->class);
    cdev_del(&i2cregmap->cdev);
    unregister_chrdev_region(i2cregmap->devid,I2CREGMAP_CNT);
    printk("i2cregmap_remove success!\n");
    return 0;
}

const struct of_device_id i2cregmap_of_table[] = {
    { .compatible = "alientek,ap3216c" },
    {}
};
const struct i2c_device_id i2cregmap_id_table[] = {
    { "ap3216c",0 },
    {}
};

struct i2c_driver i2cregmap_driver = {
    .probe = i2cregmap_probe,
    .remove = i2cregmap_remove,
    .driver = {
        .owner = THIS_MODULE,
        .name = "ap3216c",
        .of_match_table = i2cregmap_of_table
    },
    .id_table = i2cregmap_id_table
};

static int __init i2cregmap_init(void)
{
    return i2c_add_driver(&i2cregmap_driver);
}
static void __exit i2cregmap_exit(void)
{
    i2c_del_driver(&i2cregmap_driver);
}

module_init(i2cregmap_init);
module_exit(i2cregmap_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("dong");