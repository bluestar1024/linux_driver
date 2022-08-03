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
#include "ap3216creg.h"

#define  AP3216C_NAME           "ap3216c"
#define  AP3216C_CNT            1

struct ap3216c_dev{
    dev_t devid;
    int major;
    int minor;
    struct cdev cdev;
    struct class *class;
    struct device *device;
    struct i2c_client *client;
    u16 ir,als,ps;
};
struct ap3216c_dev *ap3216c;

void ap3216c_read_regs(struct i2c_client *client,u8 reg,void *buf,u16 len)
{
    struct i2c_msg msg[2];
    msg[0].addr = client->addr;
    msg[0].flags = 0;
    msg[0].buf = &reg;
    msg[0].len = 1;
    msg[1].addr = client->addr;
    msg[1].flags = I2C_M_RD;
    msg[1].buf = buf;
    msg[1].len = len;
    i2c_transfer(client->adapter,msg,2);
}
void ap3216c_write_regs(struct i2c_client *client,u8 reg,void *buf,u16 len)
{
    u8 b[len+1];
    struct i2c_msg msg;
    b[0] = reg;
    memcpy(&b[1],buf,len);

    msg.addr = client->addr;
    msg.flags = 0;
    msg.buf = b;
    msg.len = len+1;
    i2c_transfer(client->adapter,&msg,1);
}
u8 ap3216c_read_reg(struct i2c_client *client,u8 reg)
{
    u8 data = 0;
    ap3216c_read_regs(client,reg,&data,1);
    return data;
}
void ap3216c_write_reg(struct i2c_client *client,u8 reg,u8 data)
{
    ap3216c_write_regs(client,reg,&data,1);
}
void ap3216c_read_data(struct ap3216c_dev *dev)
{
    u8 buf[6];
    int i = 0;
    for(;i<6;i++)
        buf[i] = ap3216c_read_reg(dev->client,IR_DATA_LOW+i);
#if 0
    ap3216c_read_regs(dev->client,IR_DATA_LOW,buf,6);   //error!
#endif

    if(buf[0]&0x80)
        dev->ir = 0;
    else
        dev->ir = ((u16)buf[1] << 2) | (buf[0] & 0x03);

    dev->als = ((u16)buf[3] << 8) | buf[2];

    if(buf[4] & 0x40)
        dev->ps = 0;
    else
        dev->ps = (((u16)buf[5] & 0x3f) << 4) | (buf[4] & 0x0f);
}

int ap3216c_open(struct inode *inode, struct file *filp)
{
    u8 value = 0;
    filp->private_data = ap3216c;
    ap3216c_write_reg(ap3216c->client,SYSTEM_CONFIGURATION,0x4);
    mdelay(50);
    ap3216c_write_reg(ap3216c->client,SYSTEM_CONFIGURATION,0x3);
    value = ap3216c_read_reg(ap3216c->client,SYSTEM_CONFIGURATION);
    printk("system_config reg = %#x!\n",value);
    return 0;
}
ssize_t ap3216c_read(struct file *filp, char __user *buf, size_t size, loff_t *loff)
{
    struct ap3216c_dev *dev = (struct ap3216c_dev *)filp->private_data;
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
int ap3216c_close(struct inode *inode, struct file *filp)
{
    return 0;
}

const struct file_operations ap3216c_fops = {
    .owner = THIS_MODULE,
    .open = ap3216c_open,
    .read = ap3216c_read,
    .release = ap3216c_close
};

int ap3216c_probe(struct i2c_client *client, const struct i2c_device_id *id_table)
{
    int ret = 0;
    ap3216c = (struct ap3216c_dev *)kmalloc(sizeof(struct ap3216c_dev),GFP_KERNEL);
    if(IS_ERR(ap3216c))
    {
        printk("kmalloc error!\n");
        ret = PTR_ERR(ap3216c);
        return ret;
    }
    ap3216c->client = client;

    ap3216c->major = 0;
    ap3216c->minor = 0;
    if(ap3216c->major)
    {
        ap3216c->devid = MKDEV(ap3216c->major,ap3216c->minor);
        ret = register_chrdev_region(ap3216c->devid,AP3216C_CNT,AP3216C_NAME);
        if(ret < 0)
        {
            printk("register_chrdev_region error!\n");
            goto register_chrdev_error;
        }
    }
    else
    {
        ret = alloc_chrdev_region(&ap3216c->devid,ap3216c->minor,AP3216C_CNT,AP3216C_NAME);
        if(ret < 0)
        {
            printk("alloc_chrdev_region error!\n");
            goto register_chrdev_error;
        }
        ap3216c->major = MAJOR(ap3216c->devid);
        ap3216c->minor = MINOR(ap3216c->devid);
    }
    ap3216c->cdev.owner = THIS_MODULE;
    cdev_init(&ap3216c->cdev,&ap3216c_fops);
    ret = cdev_add(&ap3216c->cdev,ap3216c->devid,AP3216C_CNT);
    if(ret < 0)
    {
        printk("cdev_add error!\n");
        goto add_cdev_error;
    }
    ap3216c->class = class_create(THIS_MODULE,AP3216C_NAME);
    if(IS_ERR(ap3216c->class))
    {
        printk("class_create error!\n");
        ret = PTR_ERR(ap3216c->class);
        goto create_class_error;
    }
    ap3216c->device = device_create(ap3216c->class,NULL,ap3216c->devid,NULL,AP3216C_NAME);
    if(IS_ERR(ap3216c->device))
    {
        printk("device_create error!\n");
        ret = PTR_ERR(ap3216c->device);
        goto create_device_error;
    }

    printk("ap3216c_probe success!\n");
    return 0;

    create_device_error:
        class_destroy(ap3216c->class);
    create_class_error:
        cdev_del(&ap3216c->cdev);
    add_cdev_error:
        unregister_chrdev_region(ap3216c->devid,AP3216C_CNT);
    register_chrdev_error:
        kfree(ap3216c);
    return ret;
}
int ap3216c_remove(struct i2c_client *client)
{
    device_destroy(ap3216c->class,ap3216c->devid);
    class_destroy(ap3216c->class);
    cdev_del(&ap3216c->cdev);
    unregister_chrdev_region(ap3216c->devid,AP3216C_CNT);
    kfree(ap3216c);
    printk("ap3216c_remove success!\n");
    return 0;
}

const struct i2c_device_id ap3216c_id_table[] = {
    { "ap3216c",0 },
    {}
};
const struct of_device_id ap3216c_of_table[] = {
    { .compatible = "alientek,ap3216c" },
    {}
};

struct i2c_driver ap3216c_drv = {
    .probe = ap3216c_probe,
    .remove = ap3216c_remove,
    .driver = {
        .name = "ap3216c",
        .owner = THIS_MODULE,
        .of_match_table = ap3216c_of_table
    },
    .id_table = ap3216c_id_table
};

static int __init ap3216c_init(void)
{
    return i2c_add_driver(&ap3216c_drv);
}
static void __exit ap3216c_exit(void)
{
    i2c_del_driver(&ap3216c_drv);
}

module_init(ap3216c_init);
module_exit(ap3216c_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("dong");