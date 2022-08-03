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
#include <linux/iio/iio.h>
#include "icm20608reg.h"

#define ICM20608_TEMP_OFFSET        0
#define ICM20608_TEMP_SCALE         326800000

#define spiiio_channels_setup(_type,_iio_mod,_scan_index)                           \
    {                                                                               \
        .type = _type,                                                              \
        .modified = 1,                                                              \
        .channel2 = _iio_mod,                                                       \
        .info_mask_shared_by_type = BIT(IIO_CHAN_INFO_SCALE),                       \
        .info_mask_separate = BIT(IIO_CHAN_INFO_RAW) | BIT(IIO_CHAN_INFO_CALIBBIAS),\
        .scan_index = _scan_index,                                                  \
        .scan_type = {                                                              \
            .sign = 's',                                                            \
            .realbits = 16,                                                         \
            .storagebits = 16,                                                      \
            .shift = 0,                                                             \
            .endianness = IIO_BE                                                    \
        }                                                                           \
    }

struct spiiio_dev{
    struct regmap *regmap;
    struct regmap_config regmap_config;
    struct mutex lock;
};
struct spiiio_dev *spiiio;
struct iio_dev *indio_dev;

static const int icm20608_gyro_scale[] = {7629,15259,30518,61035};
static const int icm20608_accel_scale[] = {61035,122070,244141,488281};

struct iio_chan_spec spiiio_channels[] = {
    {
        .type = IIO_TEMP,
        .info_mask_separate = BIT(IIO_CHAN_INFO_RAW) | BIT(IIO_CHAN_INFO_SCALE) | BIT(IIO_CHAN_INFO_OFFSET),
        .scan_index = 0,
        .scan_type = {
            .sign = 's',
            .realbits = 16,
            .storagebits = 16,
            .shift = 0,
            .endianness = IIO_BE
        }
    },
    spiiio_channels_setup(IIO_ACCEL,IIO_MOD_X,1),
    spiiio_channels_setup(IIO_ACCEL,IIO_MOD_Y,2),
    spiiio_channels_setup(IIO_ACCEL,IIO_MOD_Z,3),
    spiiio_channels_setup(IIO_ANGL_VEL,IIO_MOD_X,4),
    spiiio_channels_setup(IIO_ANGL_VEL,IIO_MOD_Y,5),
    spiiio_channels_setup(IIO_ANGL_VEL,IIO_MOD_Z,6)
};

u8 icm20608_read_reg(struct spiiio_dev *dev, u8 reg)
{
    u32 data = 0;
    regmap_read(dev->regmap,reg,&data);
    return (u8)data;
}
void icm20608_write_reg(struct spiiio_dev *dev, u8 reg, u8 value)
{
    regmap_write(dev->regmap,reg,value);
}
void icm20608_read_regs(struct spiiio_dev *dev, u8 reg, u8 *buf, u8 len)
{
    regmap_bulk_read(dev->regmap,reg,buf,len);
}
void icm20608_write_regs(struct spiiio_dev *dev, u8 reg, u8 *buf, u8 len)
{
    regmap_bulk_write(dev->regmap,reg,buf,len);
}

void icm20608_init(struct spiiio_dev *dev)
{
    u8 value = 0;
    icm20608_write_reg(dev, ICM20_PWR_MGMT_1, 0x80);
    mdelay(50);
    icm20608_write_reg(dev, ICM20_PWR_MGMT_1, 0x01);
    mdelay(50);

    value = icm20608_read_reg(dev, ICM20_WHO_AM_I);
    printk("icm20608 id = %#X!\n",value);
    value = icm20608_read_reg(dev, ICM20_PWR_MGMT_1);
    printk("icm20608 ICM20_PWR_MGMT_1 = %#X!\n",value);

    icm20608_write_reg(dev, ICM20_SMPLRT_DIV, 0x00);
    icm20608_write_reg(dev, ICM20_GYRO_CONFIG, 0x18);
    icm20608_write_reg(dev, ICM20_ACCEL_CONFIG, 0x18);
    icm20608_write_reg(dev, ICM20_CONFIG, 0x04);
    icm20608_write_reg(dev, ICM20_ACCEL_CONFIG2, 0x04);
    icm20608_write_reg(dev, ICM20_LP_MODE_CFG, 0x00);
    icm20608_write_reg(dev, ICM20_FIFO_EN, 0x00);
    icm20608_write_reg(dev, ICM20_PWR_MGMT_2, 0x00);
}

int icm20608_read_data(struct spiiio_dev *dev,int iio_mod_value,u8 reg,int *val)
{
    int i = 0;
    __be16 data = 0;

    i = (iio_mod_value - IIO_MOD_X) * 2;
    icm20608_read_regs(dev,reg+i,(u8 *)&data,2);
    *val = (short)be16_to_cpup(&data);

    return IIO_VAL_INT;
}
int icm20608_read_channel_data(struct spiiio_dev *dev,struct iio_chan_spec const *chan,int *val)
{
    int ret = 0;

    switch(chan->type)
    {
        case IIO_ACCEL:
            ret = icm20608_read_data(dev,chan->channel2,ICM20_ACCEL_XOUT_H,val);
            break;
        case IIO_TEMP:
            ret = icm20608_read_data(dev,IIO_MOD_X,ICM20_TEMP_OUT_H,val);
            break;
        case IIO_ANGL_VEL:
            ret = icm20608_read_data(dev,chan->channel2,ICM20_GYRO_XOUT_H,val);
            break;
        default:
            ret = -EINVAL;
            break;
    }
    return ret;
}
int icm20608_write_data(struct spiiio_dev *dev,int iio_mod_value,u8 reg,int val)
{
    int i = 0;
    __be16 data = cpu_to_be16(val);

    i = (iio_mod_value - IIO_MOD_X) * 2;
    icm20608_write_regs(dev,reg+i,(u8 *)&data,2);
    return 0;
}
int icm20608_write_accel_scale(struct spiiio_dev *dev,int val)
{
    u8 i = 0,data = 0;

    for(i = 0;i < ARRAY_SIZE(icm20608_accel_scale);i++)
    {
        if(icm20608_accel_scale[i] == val)
        {
            data = icm20608_read_reg(dev,ICM20_ACCEL_CONFIG);
            data &=~ 0x18;
            data |= (i << 3);
            icm20608_write_reg(dev,ICM20_ACCEL_CONFIG,data);

            return 0;
        }
    }
    return -EINVAL;
}
int icm20608_write_gyro_scale(struct spiiio_dev *dev,int val)
{
    u8 i = 0,data = 0;

    for(i = 0;i < ARRAY_SIZE(icm20608_gyro_scale);i++)
    {
        if(icm20608_gyro_scale[i] == val)
        {
            data = icm20608_read_reg(dev,ICM20_GYRO_CONFIG);
            data &=~ 0x18;
            data |= (i << 3);
            icm20608_write_reg(dev,ICM20_GYRO_CONFIG,data);

            return 0;
        }
    }
    return -EINVAL;
}

int spiiio_read(struct iio_dev *indio_dev,struct iio_chan_spec const *chan,int *val,int *val2,long mask)
{
    int ret = 0;
    u8 i = 0;
    struct spiiio_dev *dev = (struct spiiio_dev *)iio_priv(indio_dev);

    switch(mask)
    {
        case IIO_CHAN_INFO_RAW:
            mutex_lock(&dev->lock);
            ret = icm20608_read_channel_data(dev,chan,val);
            mutex_unlock(&dev->lock);
            break;
        
        case IIO_CHAN_INFO_SCALE:
            switch(chan->type)
            {
                case IIO_ACCEL:
                    mutex_lock(&dev->lock);
                    i = (icm20608_read_reg(dev,ICM20_ACCEL_CONFIG) & 0x18) >> 3;
                    *val = 0;
                    *val2 = icm20608_accel_scale[i];
                    ret = IIO_VAL_INT_PLUS_NANO;
                    mutex_unlock(&dev->lock);
                    break;
                case IIO_TEMP:
                    mutex_lock(&dev->lock);
                    *val = ICM20608_TEMP_SCALE / 1000000;
                    *val2 = ICM20608_TEMP_SCALE % 1000000;
                    ret = IIO_VAL_INT_PLUS_MICRO;
                    mutex_unlock(&dev->lock);
                    break;
                case IIO_ANGL_VEL:
                    mutex_lock(&dev->lock);
                    i = (icm20608_read_reg(dev,ICM20_GYRO_CONFIG) & 0x18) >> 3;
                    *val = 0;
                    *val2 = icm20608_gyro_scale[i];
                    ret = IIO_VAL_INT_PLUS_MICRO;
                    mutex_unlock(&dev->lock);
                    break;
                default:
                    ret = -EINVAL;
                    break;
            }
            break;

        case IIO_CHAN_INFO_OFFSET:
            switch(chan->type)
            {
                case IIO_TEMP:
                    mutex_lock(&dev->lock);
                    *val = ICM20608_TEMP_OFFSET;
                    ret = IIO_VAL_INT;
                    mutex_unlock(&dev->lock);
                    break;
                default:
                    ret = -EINVAL;
                    break;
            }
            break;

        case IIO_CHAN_INFO_CALIBBIAS:
            switch(chan->type)
            {
                case IIO_ACCEL:
                    mutex_lock(&dev->lock);
                    ret = icm20608_read_data(dev,chan->channel2,ICM20_XA_OFFSET_H,val);
                    mutex_unlock(&dev->lock);
                    break;
                case IIO_ANGL_VEL:
                    mutex_lock(&dev->lock);
                    ret = icm20608_read_data(dev,chan->channel2,ICM20_XG_OFFS_USRH,val);
                    mutex_unlock(&dev->lock);
                    break;
                default:
                    ret = -EINVAL;
                    break;
            }
            break;

        default:
            ret = -EINVAL;
            break;
    }
    return ret;
}
int spiiio_write(struct iio_dev *indio_dev,struct iio_chan_spec const *chan,int val,int val2,long mask)
{
    int ret = 0;
    struct spiiio_dev *dev = (struct spiiio_dev *)iio_priv(indio_dev);

    switch(mask)
    {
        case IIO_CHAN_INFO_SCALE:
            switch(chan->type)
            {
                case IIO_ACCEL:
                    mutex_lock(&dev->lock);
                    ret = icm20608_write_accel_scale(dev,val2);
                    mutex_unlock(&dev->lock);
                    break;
                case IIO_ANGL_VEL:
                    mutex_lock(&dev->lock);
                    ret = icm20608_write_gyro_scale(dev,val2);
                    mutex_unlock(&dev->lock);
                    break;
                default:
                    ret = -EINVAL;
                    break;
            }
            break;

        case IIO_CHAN_INFO_CALIBBIAS:
            switch(chan->type)
            {
                case IIO_ACCEL:
                    mutex_lock(&dev->lock);
                    ret = icm20608_write_data(dev,chan->channel2,ICM20_XA_OFFSET_H,val);
                    mutex_unlock(&dev->lock);
                    break;
                case IIO_ANGL_VEL:
                    mutex_lock(&dev->lock);
                    ret = icm20608_write_data(dev,chan->channel2,ICM20_XG_OFFS_USRH,val);
                    mutex_unlock(&dev->lock);
                    break;
                default:
                    ret = -EINVAL;
                    break;
            }
            break;

        default:
            ret = -EINVAL;
            break;
    }
    return ret;
}
int spiiio_write_fmt(struct iio_dev *indio_dev,struct iio_chan_spec const *chan,long mask)
{
    switch(mask)
    {
        case IIO_CHAN_INFO_SCALE:
            switch(chan->type)
            {
                case IIO_ACCEL:
                    return IIO_VAL_INT_PLUS_NANO;
                case IIO_ANGL_VEL:
                    return IIO_VAL_INT_PLUS_MICRO;
                default:
                    return IIO_VAL_INT_PLUS_MICRO;
            }

        case IIO_CHAN_INFO_CALIBBIAS:
            return IIO_VAL_INT;
        
        default:
            return IIO_VAL_INT;
    }
}

const struct iio_info spiiio_info = {
    .read_raw = spiiio_read,
    .write_raw = spiiio_write,
    .write_raw_get_fmt = spiiio_write_fmt
};

int spiiio_probe(struct spi_device *spi)
{
    int ret = 0;

    indio_dev = devm_iio_device_alloc(&spi->dev,sizeof(struct spiiio_dev));
    if(IS_ERR(indio_dev))
    {
        printk("devm_iio_device_alloc error!\n");
        ret = PTR_ERR(indio_dev);
        return ret;
    }
    spiiio = (struct spiiio_dev *)iio_priv(indio_dev);

    indio_dev->dev.parent = &spi->dev;
    indio_dev->name = "spiiio_icm20608";
    indio_dev->info = &spiiio_info;
    indio_dev->modes = INDIO_DIRECT_MODE;
    indio_dev->channels = spiiio_channels;
    indio_dev->num_channels = ARRAY_SIZE(spiiio_channels);

    ret = iio_device_register(indio_dev);
    if(ret < 0)
    {
        printk("iio_device_register error!\n");
        return ret;
    }

    spi->mode = SPI_MODE_0;
    spi_setup(spi);

    spiiio->regmap_config.reg_bits = 8;
    spiiio->regmap_config.val_bits = 8;
    spiiio->regmap_config.read_flag_mask = 0x80;

    spiiio->regmap = regmap_init_spi(spi,&spiiio->regmap_config);
    if(IS_ERR(spiiio->regmap))
    {
        printk("regmap_init_spi error!\n");
        ret = PTR_ERR(spiiio->regmap);
        goto regmap_init_spi_err;
    }

    mutex_init(&spiiio->lock);

    icm20608_init(spiiio);

    printk("spiiio_probe success!\n");
    return 0;

    regmap_init_spi_err:
        iio_device_unregister(indio_dev);
    return ret;
}
int spiiio_remove(struct spi_device *spi)
{
    regmap_exit(spiiio->regmap);
    iio_device_unregister(indio_dev);
    printk("spiiio_remove success!\n");
    return 0;
}

const struct of_device_id spiiio_of_table[] = {
    { .compatible = "alientek,icm20608" },
    {}
};
const struct spi_device_id spiiio_id_table[] = {
    { "icm20608",0 },
    {}
};

struct spi_driver spiiio_driver = {
    .driver = {
        .name = "icm20608",
        .owner = THIS_MODULE,
        .of_match_table = spiiio_of_table
    },
    .id_table = spiiio_id_table,
    .probe = spiiio_probe,
    .remove = spiiio_remove
};

static int __init spiiio_init(void)
{
    return spi_register_driver(&spiiio_driver);
}
static void __exit spiiio_exit(void)
{
    spi_unregister_driver(&spiiio_driver);
}

module_init(spiiio_init);
module_exit(spiiio_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("dong");