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

#define MAX_SUPPORT_POINTS		5			/* 5点触摸 	*/
#define TOUCH_EVENT_DOWN		0x00		/* 按下 	*/
#define TOUCH_EVENT_UP			0x01		/* 抬起 	*/
#define TOUCH_EVENT_ON			0x02		/* 接触 	*/
#define TOUCH_EVENT_RESERVED	0x03		/* 保留 	*/

/* FT5X06寄存器相关宏定义 */
#define FT5X06_TD_STATUS_REG	0X02		/*	状态寄存器地址 		*/
#define FT5x06_DEVICE_MODE_REG	0X00 		/* 模式寄存器 			*/
#define FT5426_IDG_MODE_REG		0XA4		/* 中断模式				*/
#define FT5X06_READLEN			29			/* 要读取的寄存器个数 	*/

#define  FT5426_RESET_GPIO_NAME                 "ft5426_reset_gpio"
#define  FT5426_IRQ_GPIO_NAME                   "ft5426_irq_gpio"

struct ft5426_dev{
    struct i2c_client *client;
    struct device_node *dn;
    int reset_gpio;
    int irq_gpio;
    struct input_dev *inputdev;
};
struct ft5426_dev *ft5426;

void ft5426_read_regs(struct i2c_client *client,u8 reg,void *buf,u16 len)
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
void ft5426_write_regs(struct i2c_client *client,u8 reg,void *buf,u16 len)
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
u8 ft5426_read_reg(struct i2c_client *client,u8 reg)
{
    u8 data = 0;
    ft5426_read_regs(client,reg,&data,1);
    return data;
}
void ft5426_write_reg(struct i2c_client *client,u8 reg,u8 data)
{
    ft5426_write_regs(client,reg,&data,1);
}

irqreturn_t ft5426_irq_handler(int irq, void *dev_id)
{
    u8 rdbuf[FT5X06_READLEN];
    u8 *buf;
    int i = 0;
    int tplen = 6,offset = 1;
    int x = 0,y = 0;
    u8 touch_flag = 0,touch_id = 0;
    bool touch;
    struct ft5426_dev *dev = (struct ft5426_dev *)dev_id;
    memset(rdbuf,0,sizeof(rdbuf));

    ft5426_read_regs(dev->client,FT5X06_TD_STATUS_REG,rdbuf,sizeof(rdbuf));
    for(;i < MAX_SUPPORT_POINTS;i++)
    {
        buf = &rdbuf[i * tplen + offset];
        touch_flag = buf[0] >> 6;
        if(touch_flag == TOUCH_EVENT_RESERVED)
            continue;
        touch_id = buf[2] >> 4;
        touch = touch_flag != TOUCH_EVENT_UP;
        x = ((u16)(buf[2] & 0x0f)) << 8 | buf[3];
        y = ((u16)(buf[0] & 0x0f)) << 8 | buf[1];

        input_mt_slot(ft5426->inputdev,touch_id);
        input_mt_report_slot_state(ft5426->inputdev,MT_TOOL_FINGER,touch);

        if (!touch)
			continue;

        input_report_abs(ft5426->inputdev,ABS_MT_POSITION_X,x);
        input_report_abs(ft5426->inputdev,ABS_MT_POSITION_Y,y);
    }

    input_mt_report_pointer_emulation(ft5426->inputdev,true);
    input_sync(ft5426->inputdev);
    return IRQ_HANDLED;
}

void ft5426_reset_init(struct i2c_client *client,struct ft5426_dev *dev)
{
    int ret = 0;
    if(gpio_is_valid(dev->reset_gpio))
    {
        ret = devm_gpio_request_one(&client->dev,dev->reset_gpio,GPIOF_OUT_INIT_LOW,FT5426_RESET_GPIO_NAME);
        if(ret < 0)
            dev_err(&client->dev,"reset devm_gpio_request_one error!\n");
        mdelay(5);
        gpio_set_value(dev->reset_gpio,1);
        mdelay(300);
    }
}
void ft5426_irq_init(struct i2c_client *client,struct ft5426_dev *dev)
{
    int ret = 0;
    if(gpio_is_valid(dev->irq_gpio))
    {
        ret = devm_gpio_request_one(&client->dev,dev->irq_gpio,GPIOF_IN,FT5426_IRQ_GPIO_NAME);
        if(ret < 0)
            dev_err(&client->dev,"irq devm_gpio_request_one error!\n");
    }

    ret = devm_request_threaded_irq(&client->dev,client->irq,NULL,ft5426_irq_handler,IRQF_TRIGGER_FALLING|IRQF_ONESHOT,client->name,dev);
    if(ret < 0)
        dev_err(&client->dev,"irq devm_request_threaded_irq error!\n");
}

int ft5426_probe(struct i2c_client *client, const struct i2c_device_id *id_table)
{
    int ret = 0;
    u8 value = 0;
    ft5426 = (struct ft5426_dev *)devm_kzalloc(&client->dev,sizeof(struct ft5426_dev),GFP_KERNEL);
    ft5426->client = client;

    ft5426->dn = client->dev.of_node;
    ft5426->reset_gpio = of_get_named_gpio(ft5426->dn,"reset-gpios",0);
    ft5426->irq_gpio = of_get_named_gpio(ft5426->dn,"interrupt-gpios",0);

    ft5426_reset_init(client,ft5426);
    ft5426_irq_init(client,ft5426);

    ft5426_write_reg(client,FT5x06_DEVICE_MODE_REG,0);
    ft5426_write_reg(client,FT5426_IDG_MODE_REG,1);
    value = ft5426_read_reg(client,FT5426_IDG_MODE_REG);
    printk("FT5426_IDG_MODE_REG = %#x!\n",value);

    ft5426->inputdev = devm_input_allocate_device(&client->dev);
    ft5426->inputdev->name = client->name;
    ft5426->inputdev->id.bustype = BUS_I2C;
    ft5426->inputdev->dev.parent = &client->dev;

    __set_bit(EV_KEY,ft5426->inputdev->evbit);
    __set_bit(EV_ABS,ft5426->inputdev->evbit);
    __set_bit(BTN_TOUCH,ft5426->inputdev->keybit);

    input_set_abs_params(ft5426->inputdev,ABS_X,0,1024,0,0);
    input_set_abs_params(ft5426->inputdev,ABS_Y,0,600,0,0);
    input_set_abs_params(ft5426->inputdev,ABS_MT_POSITION_X,0,1024,0,0);
    input_set_abs_params(ft5426->inputdev,ABS_MT_POSITION_Y,0,600,0,0);
    ret = input_mt_init_slots(ft5426->inputdev,MAX_SUPPORT_POINTS,0);
    if(ret < 0)
    {
        printk("input_mt_init_slots error!\n");
        return ret;
    }
    ret = input_register_device(ft5426->inputdev);
    if(ret < 0)
    {
        printk("input_register_device error!\n");
        return ret;
    }
    printk("ft5426_probe success!\n");
    return 0;
}
int ft5426_remove(struct i2c_client *client)
{
    input_unregister_device(ft5426->inputdev);
    printk("ft5426_remove success!\n");
    return 0;
}

const struct i2c_device_id ft5426_id_table[] = {
    { "ft5426",0 },
    {}
};
const struct of_device_id ft5426_of_table[] = {
    { .compatible = "edt,edt-ft5426" },
    {}
};

struct i2c_driver ft5426_i2c_driver = {
    .probe = ft5426_probe,
    .remove = ft5426_remove,
    .driver = {
        .name = "ft5426",
        .owner = THIS_MODULE,
        .of_match_table = ft5426_of_table
    },
    .id_table = ft5426_id_table
};

static int __init ft5426_init(void)
{
    return i2c_add_driver(&ft5426_i2c_driver);
}
static void __exit ft5426_exit(void)
{
    i2c_del_driver(&ft5426_i2c_driver);
}

module_init(ft5426_init);
module_exit(ft5426_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("dong");