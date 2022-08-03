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
#include "icm20608reg.h"

#define  ICM20608_CNT               1
#define  ICM20608_NAME              "icm20608"

#define  ICM20608_CS_GPIO           "cs-gpio"

struct icm20608_dev{
    dev_t devid;
    int major;
    int minor;
    struct cdev cdev;
    struct class *class;
    struct device *device;
    struct spi_device *spi;
    struct device_node *dn;
    int cs_gpio;
    
    short accel_x_adc;
    short accel_y_adc;
    short accel_z_adc;
    short temp_adc;
    short gyro_x_adc;
    short gyro_y_adc;
    short gyro_z_adc;
};
struct icm20608_dev *icm20608;

int icm20608_read_regs(struct icm20608_dev *dev, u8 reg, void *buf, int len)
{
    int ret = 0;
    struct spi_transfer t;
    struct spi_message m;
    reg |= 0x80;
    memset(&t,0,sizeof(struct spi_transfer));
    
    gpio_set_value(dev->cs_gpio,0);
    t.tx_buf = &reg;
    t.len = 1;
    spi_message_init(&m);
    spi_message_add_tail(&t,&m);
    ret = spi_sync(dev->spi,&m);
    if(ret < 0)
    {
        printk("icm20608_read_regs tx error!\n");
        return ret;
    }

    t.rx_buf = buf;
    t.len = len;
    spi_message_init(&m);
    spi_message_add_tail(&t,&m);
    ret = spi_sync(dev->spi,&m);
    if(ret < 0)
    {
        printk("icm20608_read_regs rx error!\n");
        return ret;
    }
    gpio_set_value(dev->cs_gpio,1);
    return 0;
}
int icm20608_write_regs(struct icm20608_dev *dev, u8 reg, void *buf, int len)
{
    int ret = 0;
    u8 txdata[len + 1];
    struct spi_transfer t;
    struct spi_message m;
    txdata[0] = reg & ~0x80;
    memcpy(&txdata[1],buf,len);
    memset(&t,0,sizeof(struct spi_transfer));

    gpio_set_value(dev->cs_gpio,0);
    t.tx_buf = txdata;
    t.len = len + 1;
    spi_message_init(&m);
    spi_message_add_tail(&t,&m);
    ret = spi_sync(dev->spi,&m);
    if(ret < 0)
    {
        printk("icm20608_write_regs error!\n");
        return ret;
    }
    gpio_set_value(dev->cs_gpio,1);
    return 0;
}
u8 icm20608_read_reg(struct icm20608_dev *dev, u8 reg)
{
    u8 data = 0;
    icm20608_read_regs(dev, reg, &data, 1);
    return data;
}
void icm20608_write_reg(struct icm20608_dev *dev, u8 reg, u8 data)
{
    icm20608_write_regs(dev, reg, &data, 1);
}
void icm20608_read_data(struct icm20608_dev *dev)
{
    u8 buf[14];
    icm20608_read_regs(dev, ICM20_ACCEL_XOUT_H, buf, sizeof(buf));
    
    dev->accel_x_adc = (short) (((u16)buf[0] << 8) | buf[1]);
    dev->accel_y_adc = (short) (((u16)buf[2] << 8) | buf[3]);
    dev->accel_z_adc = (short) (((u16)buf[4] << 8) | buf[5]);
    dev->temp_adc    = (short) (((u16)buf[6] << 8) | buf[7]);
    dev->gyro_x_adc  = (short) (((u16)buf[8] << 8) | buf[9]);
    dev->gyro_y_adc  = (short) (((u16)buf[10] << 8) | buf[11]);
    dev->gyro_z_adc  = (short) (((u16)buf[12] << 8) | buf[13]);
}

int icm20608_open(struct inode *inode, struct file *filp)
{
    u8 value = 0;
    filp->private_data = icm20608;
    icm20608_write_reg(icm20608, ICM20_PWR_MGMT_1, 0x80);
    mdelay(50);
    icm20608_write_reg(icm20608, ICM20_PWR_MGMT_1, 0x01);
    mdelay(50);

    value = icm20608_read_reg(icm20608, ICM20_WHO_AM_I);
    printk("icm20608 id = %#X!\n",value);
    value = icm20608_read_reg(icm20608, ICM20_PWR_MGMT_1);
    printk("icm20608 ICM20_PWR_MGMT_1 = %#X!\n",value);

    icm20608_write_reg(icm20608, ICM20_SMPLRT_DIV, 0x00);
    icm20608_write_reg(icm20608, ICM20_GYRO_CONFIG, 0x18);
    icm20608_write_reg(icm20608, ICM20_ACCEL_CONFIG, 0x18);
    icm20608_write_reg(icm20608, ICM20_CONFIG, 0x04);
    icm20608_write_reg(icm20608, ICM20_ACCEL_CONFIG2, 0x04);
    icm20608_write_reg(icm20608, ICM20_LP_MODE_CFG, 0x00);
    icm20608_write_reg(icm20608, ICM20_FIFO_EN, 0x00);
    icm20608_write_reg(icm20608, ICM20_PWR_MGMT_2, 0x00);
    return 0;
}
ssize_t icm20608_read(struct file *filp, char __user *buf, size_t size, loff_t *loff)
{
    short data[7];
    int ret = 0;
    struct icm20608_dev *dev = (struct icm20608_dev *)filp->private_data;
    icm20608_read_data(dev);
    data[0] = dev->accel_x_adc;
    data[1] = dev->accel_y_adc;
    data[2] = dev->accel_z_adc;
    data[3] = dev->gyro_x_adc;
    data[4] = dev->gyro_y_adc;
    data[5] = dev->gyro_z_adc;
    data[6] = dev->temp_adc;
    ret = copy_to_user(buf,data,size);
    if(ret < 0)
    {
        printk("copy_to_user error!\n");
        return ret;
    }
    return 0;
}
int icm20608_close(struct inode *inode, struct file *filp)
{
    return 0;
}

const struct file_operations icm20608_fops = {
    .owner = THIS_MODULE,
    .open = icm20608_open,
    .read = icm20608_read,
    .release = icm20608_close
};

int icm20608_cs_gpio_init(struct spi_device *spi)
{
    int ret = 0;
    icm20608->dn = of_get_parent(spi->dev.of_node);
    if(IS_ERR(icm20608->dn))
    {
        printk("of_get_parent error!\n");
        ret = PTR_ERR(icm20608->dn);
        return ret;
    }
    icm20608->cs_gpio = of_get_named_gpio(icm20608->dn,ICM20608_CS_GPIO,0);
    if(icm20608->cs_gpio < 0)
    {
        printk("of_get_named_gpio error!\n");
        ret = icm20608->cs_gpio;
        return ret;
    }
    printk("cs-gpio = %d!\n",icm20608->cs_gpio);
    ret = gpio_request(icm20608->cs_gpio,ICM20608_CS_GPIO);
    if(ret < 0)
    {
        printk("gpio_request error!\n");
        return ret;
    }
    ret = gpio_direction_output(icm20608->cs_gpio,1);
    if(ret < 0)
    {
        printk("gpio_direction_output error!\n");
        goto gpio_output_error;
    }
    return 0;

    gpio_output_error:
        gpio_free(icm20608->cs_gpio);
    return ret;
}

int icm20608_probe(struct spi_device *spi)
{
    int ret = 0;
    icm20608 = (struct icm20608_dev *)kmalloc(sizeof(struct icm20608_dev),GFP_KERNEL);
    if(IS_ERR(icm20608))
    {
        printk("kmalloc error!\n");
        ret = PTR_ERR(icm20608);
        return ret;
    }
    icm20608->spi = spi;
    spi->mode = SPI_MODE_0;
    spi_setup(spi);

    icm20608->major = 0;
    icm20608->minor = 0;
    if(icm20608->major)
    {
        icm20608->devid = MKDEV(icm20608->major,icm20608->minor);
        ret = register_chrdev_region(icm20608->devid,ICM20608_CNT,ICM20608_NAME);
        if(ret < 0)
        {
            printk("register_chrdev_region error!\n");
            goto register_chrdev_error;
        }
    }
    else
    {
        ret = alloc_chrdev_region(&icm20608->devid,icm20608->minor,ICM20608_CNT,ICM20608_NAME);
        if(ret < 0)
        {
            printk("alloc_chrdev_region error!\n");
            goto register_chrdev_error;
        }
        icm20608->major = MAJOR(icm20608->devid);
        icm20608->minor = MINOR(icm20608->devid);
    }
    icm20608->cdev.owner = THIS_MODULE;
    cdev_init(&icm20608->cdev,&icm20608_fops);
    ret = cdev_add(&icm20608->cdev,icm20608->devid,ICM20608_CNT);
    if(ret < 0)
    {
        printk("cdev_add error!\n");
        goto add_cdev_error;
    }
    icm20608->class = class_create(THIS_MODULE,ICM20608_NAME);
    if(IS_ERR(icm20608->class))
    {
        printk("class_create error!\n");
        ret = PTR_ERR(icm20608->class);
        goto create_class_error;
    }
    icm20608->device = device_create(icm20608->class,NULL,icm20608->devid,NULL,ICM20608_NAME);
    if(IS_ERR(icm20608->device))
    {
        printk("device_create error!\n");
        ret = PTR_ERR(icm20608->device);
        goto create_device_error;
    }

    ret = icm20608_cs_gpio_init(spi);
    if(ret < 0)
    {
        printk("icm20608_cs_gpio_init error!\n");
        goto cs_gpio_init_error;
    }
    printk("icm20608_probe success!\n");
    return 0;

    cs_gpio_init_error:
        device_destroy(icm20608->class,icm20608->devid);
    create_device_error:
        class_destroy(icm20608->class);
    create_class_error:
        cdev_del(&icm20608->cdev);
    add_cdev_error:
        unregister_chrdev_region(icm20608->devid,ICM20608_CNT);
    register_chrdev_error:
        kfree(icm20608);
    return ret;
}
int icm20608_remove(struct spi_device *spi)
{
    gpio_free(icm20608->cs_gpio);
    device_destroy(icm20608->class,icm20608->devid);
    class_destroy(icm20608->class);
    cdev_del(&icm20608->cdev);
    unregister_chrdev_region(icm20608->devid,ICM20608_CNT);
    kfree(icm20608);
    printk("icm20608_remove success!\n");
    return 0;
}

const struct spi_device_id icm20608_id_table[] = {
    { "icm20608",0 },
    {}
};
const struct of_device_id icm20608_of_table[] = {
    { .compatible = "alientek,icm20608" },
    {}
};

struct spi_driver icm20608_driver = {
    .probe = icm20608_probe,
    .remove = icm20608_remove,
    .driver = {
        .name = "icm20608",
        .owner = THIS_MODULE,
        .of_match_table = icm20608_of_table
    },
    .id_table = icm20608_id_table
};

static int __init icm20608_xx_init(void)
{
    return spi_register_driver(&icm20608_driver);
}
static void __exit icm20608_xx_exit(void)
{
    spi_unregister_driver(&icm20608_driver);
}

module_init(icm20608_xx_init);
module_exit(icm20608_xx_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("dong");