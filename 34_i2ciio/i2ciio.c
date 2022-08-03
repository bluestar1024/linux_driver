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
#include "ap3216creg.h"

struct i2ciio_dev{
    struct regmap *regmap;
    struct regmap_config regmap_config;
    struct mutex lock;
};
struct i2ciio_dev *i2ciio;
struct iio_dev *indio_dev;

static const int ap3216c_als_scale[] = {315262,78766,19699,4929};

struct iio_chan_spec const i2ciio_channels[] = {
    {
        .type = IIO_INTENSITY,
        .modified = 1,
        .channel2 = IIO_MOD_LIGHT_IR,
        .address = 0X0A,
        .info_mask_separate = BIT(IIO_CHAN_INFO_RAW),
        .scan_index = 0,
        .scan_type = {
            .sign = 'u',
            .realbits = 10,
            .storagebits = 16,
            .shift = 6,
            .endianness = IIO_LE
        }
    },
    {
        .type = IIO_INTENSITY,
        .modified = 1,
        .channel2 = IIO_MOD_LIGHT_BOTH,
        .address = 0X0C,
        .info_mask_separate = BIT(IIO_CHAN_INFO_RAW) | BIT(IIO_CHAN_INFO_SCALE),
        .scan_index = 1,
        .scan_type = {
            .sign = 'u',
            .realbits = 16,
            .storagebits = 16,
            .shift = 0,
            .endianness = IIO_LE
        }
    },
    {
        .type = IIO_PROXIMITY,
        .address = 0X0E,
        .info_mask_separate = BIT(IIO_CHAN_INFO_RAW),
        .scan_index = 2,
        .scan_type = {
            .sign = 'u',
            .realbits = 10,
            .storagebits = 16,
            .shift = 6,
            .endianness = IIO_LE
        }
    }
};

u8 ap3216c_read_reg(struct i2ciio_dev *dev, u8 reg)
{
    u32 data = 0;
    regmap_read(dev->regmap,reg,&data);
    return (u8)data;
}
void ap3216c_write_reg(struct i2ciio_dev *dev, u8 reg, u8 value)
{
    regmap_write(dev->regmap,reg,value);
}
void ap3216c_read_regs(struct i2ciio_dev *dev, u8 reg, u8 *buf, u8 len)
{
    regmap_bulk_read(dev->regmap,reg,buf,len);
}
void ap3216c_write_regs(struct i2ciio_dev *dev, u8 reg, u8 *buf, u8 len)
{
    regmap_bulk_write(dev->regmap,reg,buf,len);
}

void ap3216c_init(struct i2ciio_dev *dev)
{
    u8 value = 0;
    ap3216c_write_reg(dev,SYSTEM_CONFIGURATION,0x4);        /* 复位AP3216C 			*/
    mdelay(50);                                             /* AP3216C复位最少10ms 	*/
    ap3216c_write_reg(dev,ALS_CONFIG,0X00);                 /* ALS单次转换触发，量程为0～20661 lux */
    ap3216c_write_reg(dev,SYSTEM_CONFIGURATION,0x3);        /* 开启ALS、PS+IR 		*/

    value = ap3216c_read_reg(dev,SYSTEM_CONFIGURATION);
    printk("system_config reg = %#x!\n",value);
}

int ap3216c_read_ir_data(struct i2ciio_dev *dev,struct iio_chan_spec const *chan,int *val)
{
    u8 buf[2];
#if 0
    int i = 0;
    for(;i<2;i++)
        buf[i] = ap3216c_read_reg(dev,chan->address+i);
#endif
#if 1
    ap3216c_read_regs(dev,chan->address,buf,2);
#endif

    if(buf[0]&0x80)
        *val = 0;
    else
        *val = ((u16)buf[1] << 2) | (buf[0] & 0x03);
    return IIO_VAL_INT;
}
int ap3216c_read_als_data(struct i2ciio_dev *dev,struct iio_chan_spec const *chan,int *val)
{
    u8 buf[2];
#if 0
    int i = 0;
    for(;i<2;i++)
        buf[i] = ap3216c_read_reg(dev,chan->address+i);
#endif
#if 1
    ap3216c_read_regs(dev,chan->address,buf,2);
#endif

    *val = ((u16)buf[1] << 8) | buf[0];
    return IIO_VAL_INT;
}
int ap3216c_read_ps_data(struct i2ciio_dev *dev,struct iio_chan_spec const *chan,int *val)
{
    u8 buf[2];
#if 0
    int i = 0;
    for(;i<2;i++)
        buf[i] = ap3216c_read_reg(dev,chan->address+i);
#endif
#if 1
    ap3216c_read_regs(dev,chan->address,buf,2);
#endif

    if(buf[0] & 0x40)
        *val = 0;
    else
        *val = ((u16)(buf[1] & 0x3F) << 4) | (buf[0] & 0x0F);
    return IIO_VAL_INT;
}
int ap3216c_read_als_scale(struct i2ciio_dev *dev,int *val,int *val2)
{
    int i = 0;
    i = (ap3216c_read_reg(dev,ALS_CONFIG) & 0x30) >> 4;
    *val = 0;
    *val2 = ap3216c_als_scale[i];
    return IIO_VAL_INT_PLUS_MICRO;
}
int ap3216c_write_als_scale(struct i2ciio_dev *dev,int val)
{
    int i = 0;
    u8 data = 0;

    for(;i < ARRAY_SIZE(ap3216c_als_scale);i++)
    {
        if(ap3216c_als_scale[i] == val)
        {
            data = ap3216c_read_reg(dev,ALS_CONFIG);
            data &=~ 0x30;
            data |= (i << 4);
            ap3216c_write_reg(dev,ALS_CONFIG,data);
            return 0;
        }
    }
    return -EINVAL;
}

int i2ciio_read(struct iio_dev *indio_dev,struct iio_chan_spec const *chan,int *val,int *val2,long mask)
{
    int ret = 0;
    struct i2ciio_dev *dev = (struct i2ciio_dev *)iio_priv(indio_dev);

    switch(mask)
    {
        case IIO_CHAN_INFO_RAW:
            switch(chan->type)
            {
                case IIO_INTENSITY:
                    switch(chan->channel2)
                    {
                        case IIO_MOD_LIGHT_IR:
                            mutex_lock(&i2ciio->lock);
                            ret = ap3216c_read_ir_data(dev,chan,val);
                            mutex_unlock(&i2ciio->lock);
                            break;
                        case IIO_MOD_LIGHT_BOTH:
                            mutex_lock(&i2ciio->lock);
                            ret = ap3216c_read_als_data(dev,chan,val);
                            mutex_unlock(&i2ciio->lock);
                            break;
                        default:
                            ret = -EINVAL;
                            break;
                    }
                    break;
                
                case IIO_PROXIMITY:
                    mutex_lock(&i2ciio->lock);
                    ret = ap3216c_read_ps_data(dev,chan,val);
                    mutex_unlock(&i2ciio->lock);
                    break;
                
                default:
                    ret = -EINVAL;
                    break;
            }
            break;

        case IIO_CHAN_INFO_SCALE:
            switch(chan->type)
            {
                case IIO_INTENSITY:
                    switch(chan->channel2)
                    {
                        case IIO_MOD_LIGHT_BOTH:
                            mutex_lock(&i2ciio->lock);
                            ret = ap3216c_read_als_scale(dev,val,val2);
                            mutex_unlock(&i2ciio->lock);
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
            break;
        
        default:
            ret = -EINVAL;
            break;
    }

    return ret;
}
int i2ciio_write(struct iio_dev *indio_dev,struct iio_chan_spec const *chan,int val,int val2,long mask)
{
    int ret = 0;
    struct i2ciio_dev *dev = (struct i2ciio_dev *)iio_priv(indio_dev);

    switch(mask)
    {
        case IIO_CHAN_INFO_SCALE:
            switch(chan->type)
            {
                case IIO_INTENSITY:
                    switch(chan->channel2)
                    {
                        case IIO_MOD_LIGHT_BOTH:
                            mutex_lock(&i2ciio->lock);
                            ret = ap3216c_write_als_scale(dev,val2);
                            mutex_unlock(&i2ciio->lock);
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
            break;
        
        default:
            ret = -EINVAL;
            break;
    }

    return ret;
}
int i2ciio_write_fmt(struct iio_dev *indio_dev,struct iio_chan_spec const *chan,long mask)
{
    switch(mask)
    {
        case IIO_CHAN_INFO_SCALE:
            switch(chan->type)
            {
                case IIO_INTENSITY:
                    switch(chan->channel2)
                    {
                        case IIO_MOD_LIGHT_BOTH:
                            return IIO_VAL_INT_PLUS_MICRO;
                        default:
                            return -EINVAL;
                    }
                
                default:
                    return -EINVAL;
            }
        
        default:
            return -EINVAL;
    }
}

const struct iio_info i2ciio_info = {
    .read_raw = i2ciio_read,
    .write_raw = i2ciio_write,
    .write_raw_get_fmt = i2ciio_write_fmt
};

int i2ciio_probe(struct i2c_client *i2c, const struct i2c_device_id *id)
{
    int ret = 0;

    indio_dev = devm_iio_device_alloc(&i2c->dev,sizeof(struct i2ciio_dev));
    if(IS_ERR(indio_dev))
    {
        printk("devm_iio_device_alloc error!\n");
        ret = PTR_ERR(indio_dev);
        return ret;
    }
    i2ciio = (struct i2ciio_dev *)iio_priv(indio_dev);

    indio_dev->dev.parent = &i2c->dev;
    indio_dev->modes = INDIO_DIRECT_MODE;
    indio_dev->name = "i2ciio_ap3216c";
    indio_dev->info = &i2ciio_info;
    indio_dev->channels = i2ciio_channels;
    indio_dev->num_channels = ARRAY_SIZE(i2ciio_channels);

    ret = iio_device_register(indio_dev);
    if(ret)
    {
        printk("iio_device_register error!\n");
        return ret;
    }

    i2ciio->regmap_config.reg_bits = 8;
    i2ciio->regmap_config.val_bits = 8;
    i2ciio->regmap = regmap_init_i2c(i2c,&i2ciio->regmap_config);
    if(IS_ERR(i2ciio->regmap))
    {
        printk("regmap_init_i2c error!\n");
        ret = PTR_ERR(i2ciio->regmap);
        goto regmap_init_i2c_error;
    }

    mutex_init(&i2ciio->lock);

    ap3216c_init(i2ciio);

    printk("i2ciio_probe success!\n");
    return 0;

    regmap_init_i2c_error:
        iio_device_unregister(indio_dev);
    return ret;
}
int i2ciio_remove(struct i2c_client *i2c)
{
    regmap_exit(i2ciio->regmap);
    iio_device_unregister(indio_dev);

    printk("i2ciio_remove success!\n");
    return 0;
}

const struct of_device_id i2ciio_of_table[] = {
    { .compatible = "alientek,ap3216c" },
    {}
};
const struct i2c_device_id i2ciio_id_table[] = {
    { .name = "ap3216c" },
    {}
};

struct i2c_driver i2ciio_driver = {
    .driver = {
        .name = "ap3216c",
        .of_match_table = i2ciio_of_table,
        .owner = THIS_MODULE
    },
    .id_table = i2ciio_id_table,
    .probe = i2ciio_probe,
    .remove = i2ciio_remove
};

static int __init i2ciio__init(void)
{
    return i2c_add_driver(&i2ciio_driver);
}
static void __exit i2ciio_exit(void)
{
    i2c_del_driver(&i2ciio_driver);
}

module_init(i2ciio__init);
module_exit(i2ciio_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("dong");