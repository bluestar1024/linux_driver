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
#include "icm20608reg.h"

#define  SPIREGMAP_CNT               1
#define  SPIREGMAP_NAME              "spiregmap"

struct spiregmap_dev{
    dev_t devid;
    int major;
    int minor;
    struct cdev cdev;
    struct class *class;
    struct device *device;
    struct regmap *regmap;
    struct regmap_config regmap_config;
    
    short accel_x_adc;
    short accel_y_adc;
    short accel_z_adc;
    short temp_adc;
    short gyro_x_adc;
    short gyro_y_adc;
    short gyro_z_adc;
};
struct spiregmap_dev *spiregmap;

u8 icm20608_read_reg(struct spiregmap_dev *dev, u8 reg)
{
    u32 data = 0;
    regmap_read(dev->regmap,reg,&data);
    return (u8)data;
}
void icm20608_write_reg(struct spiregmap_dev *dev, u8 reg, u8 value)
{
    regmap_write(dev->regmap,reg,value);
}
void icm20608_read_regs(struct spiregmap_dev *dev, u8 reg, u8 *buf, u8 len)
{
    regmap_bulk_read(dev->regmap,reg,buf,len);
}

void icm20608_read_data(struct spiregmap_dev *dev)
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

int spiregmap_open(struct inode * inode, struct file *filp)
{
    u8 value = 0;
    filp->private_data = spiregmap;
    icm20608_write_reg(spiregmap, ICM20_PWR_MGMT_1, 0x80);
    mdelay(50);
    icm20608_write_reg(spiregmap, ICM20_PWR_MGMT_1, 0x01);
    mdelay(50);

    value = icm20608_read_reg(spiregmap, ICM20_WHO_AM_I);
    printk("icm20608 id = %#X!\n",value);
    value = icm20608_read_reg(spiregmap, ICM20_PWR_MGMT_1);
    printk("icm20608 ICM20_PWR_MGMT_1 = %#X!\n",value);

    icm20608_write_reg(spiregmap, ICM20_SMPLRT_DIV, 0x00);
    icm20608_write_reg(spiregmap, ICM20_GYRO_CONFIG, 0x18);
    icm20608_write_reg(spiregmap, ICM20_ACCEL_CONFIG, 0x18);
    icm20608_write_reg(spiregmap, ICM20_CONFIG, 0x04);
    icm20608_write_reg(spiregmap, ICM20_ACCEL_CONFIG2, 0x04);
    icm20608_write_reg(spiregmap, ICM20_LP_MODE_CFG, 0x00);
    icm20608_write_reg(spiregmap, ICM20_FIFO_EN, 0x00);
    icm20608_write_reg(spiregmap, ICM20_PWR_MGMT_2, 0x00);
    return 0;
}
ssize_t spiregmap_read(struct file *filp, char __user *buf, size_t size, loff_t *loff)
{
    short data[7];
    int ret = 0;
    struct spiregmap_dev *dev = (struct spiregmap_dev *)filp->private_data;
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
int spiregmap_close(struct inode *inode, struct file *filp)
{
    return 0;
}

const struct file_operations spiregmap_fops = {
    .owner = THIS_MODULE,
    .open = spiregmap_open,
    .read = spiregmap_read,
    .release = spiregmap_close
};

int	spiregmap_probe(struct spi_device *spi)
{
    int ret = 0;
    spiregmap = (struct spiregmap_dev *)devm_kzalloc(&spi->dev,sizeof(struct spiregmap_dev),GFP_KERNEL);
    if(IS_ERR(spiregmap))
    {
        printk("devm_kzalloc error!\n");
        ret = PTR_ERR(spiregmap);
        return ret;
    }

    spi->mode = SPI_MODE_0;
    spi_setup(spi);

    spiregmap->major = 0;
    spiregmap->minor = 0;
    if(spiregmap->major)
    {
        spiregmap->devid = MKDEV(spiregmap->major,spiregmap->minor);
        ret = register_chrdev_region(spiregmap->devid,SPIREGMAP_CNT,SPIREGMAP_NAME);
        if(ret < 0)
        {
            printk("register_chrdev_region error!\n");
            goto register_chrdev_error;
        }
    }
    else
    {
        ret = alloc_chrdev_region(&spiregmap->devid,spiregmap->minor,SPIREGMAP_CNT,SPIREGMAP_NAME);
        if(ret < 0)
        {
            printk("alloc_chrdev_region error!\n");
            goto register_chrdev_error;
        }
        spiregmap->major = MAJOR(spiregmap->devid);
        spiregmap->minor = MINOR(spiregmap->devid);
    }

    spiregmap->cdev.owner = THIS_MODULE;
    cdev_init(&spiregmap->cdev,&spiregmap_fops);
    ret = cdev_add(&spiregmap->cdev,spiregmap->devid,SPIREGMAP_CNT);
    if(ret < 0)
    {
        printk("cdev_add error!\n");
        goto add_cdev_error;
    }

    spiregmap->class = class_create(THIS_MODULE,SPIREGMAP_NAME);
    if(IS_ERR(spiregmap->class))
    {
        printk("class_create error!\n");
        ret = PTR_ERR(spiregmap->class);
        goto create_class_error;
    }
    spiregmap->device = device_create(spiregmap->class,NULL,spiregmap->devid,NULL,SPIREGMAP_NAME);
    if(IS_ERR(spiregmap->device))
    {
        printk("device_create error!\n");
        ret = PTR_ERR(spiregmap->device);
        goto create_device_error;
    }

    spiregmap->regmap_config.reg_bits = 8;
    spiregmap->regmap_config.val_bits = 8;
    spiregmap->regmap_config.read_flag_mask = 0x80;

    spiregmap->regmap = regmap_init_spi(spi,&spiregmap->regmap_config);
    if(IS_ERR(spiregmap->regmap))
    {
        printk("regmap_init_spi error!\n");
        ret = PTR_ERR(spiregmap->regmap);
        goto regmap_init_spi_error;
    }
    printk("spiregmap_probe success!\n");
    return 0;

    regmap_init_spi_error:
        device_destroy(spiregmap->class,spiregmap->devid);
    create_device_error:
        class_destroy(spiregmap->class);
    create_class_error:
        cdev_del(&spiregmap->cdev);
    add_cdev_error:
        unregister_chrdev_region(spiregmap->devid,SPIREGMAP_CNT);
    register_chrdev_error:
    return ret;
}
int	spiregmap_remove(struct spi_device *spi)
{
    regmap_exit(spiregmap->regmap);
    device_destroy(spiregmap->class,spiregmap->devid);
    class_destroy(spiregmap->class);
    cdev_del(&spiregmap->cdev);
    unregister_chrdev_region(spiregmap->devid,SPIREGMAP_CNT);
    printk("spiregmap_remove success!\n");
    return 0;
}

const struct of_device_id spiregmap_of_table[] = {
    { .compatible = "alientek,icm20608" },
    {}
};
const struct spi_device_id spiregmap_id_table[] = {
    { "icm20608",0 },
    {}
};

struct spi_driver spiregmap_driver = {
    .probe = spiregmap_probe,
    .remove = spiregmap_remove,
    .driver = {
        .owner = THIS_MODULE,
        .name = "icm20608",
        .of_match_table = spiregmap_of_table
    },
    .id_table = spiregmap_id_table
};

static int __init spiregmap_init(void)
{
    return spi_register_driver(&spiregmap_driver);
}
static void __exit spiregmap_exit(void)
{
    spi_unregister_driver(&spiregmap_driver);
}

module_init(spiregmap_init);
module_exit(spiregmap_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("dong");