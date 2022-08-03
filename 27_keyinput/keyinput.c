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

#define  KEYINPUT_NAME          "keyinput"

#define  KEY_COUNT              1

#define  KEY_NODE               "/key"
#define  KEY_PROP               "key-gpio"

#define  LED_NODE               "/gpioled"
#define  LED_PROP               "led-gpio"

struct irq_keydesc{
    int key_gpio;
    unsigned int irq_num;
    irq_handler_t irq_handler_name;
    char name[10];
    //struct tasklet_struct tasklet;
    //void (*key_tasklet_name)(unsigned long);
    struct work_struct work;
    work_func_t key_work_name;
};
struct led_desc{
    int led_gpio;
    char name[10];
};
struct timer_desc{
    struct timer_list timer;
};
struct keyinput_dev{
    struct input_dev *inputdev;
    struct device_node *dn;
    struct irq_keydesc key[KEY_COUNT];
    struct led_desc led;
    struct timer_desc timer;
};
struct keyinput_dev *keyinput;

irqreturn_t irq_handle(int irq_num, void *dev_id)
{
    struct keyinput_dev *dev = (struct keyinput_dev *)dev_id;
    //tasklet_schedule(&dev->key[0].tasklet);
    schedule_work(&dev->key[0].work);
    return IRQ_HANDLED;
}
#if 0
void key_tasklet(unsigned long data)
{
    struct keyinput_dev *dev = (struct keyinput_dev *)data;
    dev->timer.timer.data = data;
    mod_timer(&dev->timer.timer,jiffies + msecs_to_jiffies(20));
}
#endif
void key_work(struct work_struct *work)
{
    struct irq_keydesc *key0_struct_ptr = container_of(work,struct irq_keydesc,work);
    struct keyinput_dev *dev = container_of(key0_struct_ptr,struct keyinput_dev,key[0]);
    dev->timer.timer.data = (unsigned long)dev;
    mod_timer(&dev->timer.timer,jiffies + msecs_to_jiffies(20));
}
void timer_handle(unsigned long arg)
{
    struct keyinput_dev *dev = (struct keyinput_dev *)arg;
    if(gpio_get_value(dev->key[0].key_gpio))
    {
        //printk("KEY0 release!\n");
        input_report_key(keyinput->inputdev,BTN_0,0);
        input_sync(keyinput->inputdev);
        gpio_set_value(dev->led.led_gpio,0);
    }
    else
    {
        //printk("KEY0 press!\n");
        input_event(keyinput->inputdev,EV_KEY,BTN_0,1);
        input_sync(keyinput->inputdev);
        gpio_set_value(dev->led.led_gpio,1);
    }
}

int keyio_init(void)
{
    int i = 0, j = 0;
    int ret = 0;

    keyinput->dn = of_find_node_by_path(KEY_NODE);
    if(!keyinput->dn)
    {
        printk("key:of_find_node_by_path error!\n");
        return -EINVAL;
    }

    keyinput->key[0].irq_handler_name = irq_handle;
    //keyinput->key[0].key_tasklet_name = key_tasklet;
    keyinput->key[0].key_work_name = key_work;

    for(; i < KEY_COUNT; i++)
    {
        keyinput->key[i].key_gpio = of_get_named_gpio(keyinput->dn,KEY_PROP,i);
        if(keyinput->key[i].key_gpio < 0)
        {
            printk("key:of_get_named_gpio error!\n");
            ret = keyinput->key[i].key_gpio;
            goto get_gpio_error;
        }

        memset(keyinput->key[i].name,0,sizeof(keyinput->key[i].name));
        sprintf(keyinput->key[i].name,"GPIO_KEY%d",i);
        ret = gpio_request(keyinput->key[i].key_gpio,keyinput->key[i].name);
        if(ret)
        {
            printk("key:gpio_request error!\n");
            goto request_gpio_error;
        }

        ret = gpio_direction_input(keyinput->key[i].key_gpio);
        if(ret)
        {
            printk("key:gpio_direction_input error!\n");
            goto gpio_input_error;
        }

    #if 0
        keyinput->key[i].irq_num = gpio_to_irq(keyinput->key[i].key_gpio);
    #endif
    #if 1
        keyinput->key[i].irq_num = irq_of_parse_and_map(keyinput->dn,i);
    #endif

        memset(keyinput->key[i].name,0,sizeof(keyinput->key[i].name));
        sprintf(keyinput->key[i].name,"IRQ_KEY%d",i);
        ret = request_irq(keyinput->key[i].irq_num,keyinput->key[i].irq_handler_name,IRQF_TRIGGER_RISING|IRQF_TRIGGER_FALLING,keyinput->key[i].name,keyinput);
        if(ret)
        {
            printk("request_irq error!\n");
            goto request_irq_error;
        }

        //tasklet_init(&keyinput->key[i].tasklet,keyinput->key[i].key_tasklet_name,(unsigned long)keyinput);
        INIT_WORK(&keyinput->key[i].work,keyinput->key[i].key_work_name);
    }
    return 0;

    request_irq_error:
        j = i;
        for(j = j - 1; j >= 0; j--)
            free_irq(keyinput->key[j].irq_num,keyinput);
    gpio_input_error:
        gpio_free(keyinput->key[i].key_gpio);
    request_gpio_error:
        for(i = i - 1; i >= 0; i--)
            gpio_free(keyinput->key[i].key_gpio);
    get_gpio_error:
    return ret;
}
int led_init(void)
{
    int ret = 0;
    keyinput->dn = of_find_node_by_path(LED_NODE);
    if(!keyinput->dn)
    {
        printk("led:of_find_node_by_path error!\n");
        return -EINVAL;
    }

    keyinput->led.led_gpio = of_get_named_gpio(keyinput->dn,LED_PROP,0);
    if(keyinput->led.led_gpio < 0)
    {
        printk("led:of_get_named_gpio error!\n");
        ret = keyinput->led.led_gpio;
        goto get_gpio_error;
    }

    memset(keyinput->led.name,0,sizeof(keyinput->led.name));
    sprintf(keyinput->led.name,"GPIO_LED");
    ret = gpio_request(keyinput->led.led_gpio,keyinput->led.name);
    if(ret)
    {
        printk("led:gpio_request error!\n");
        goto request_gpio_error;
    }
    ret = gpio_direction_output(keyinput->led.led_gpio,1);
    if(ret)
    {
        printk("led:gpio_direction_output error!\n");
        goto gpio_output_error;
    }
    gpio_set_value(keyinput->led.led_gpio,0);
    return 0;

    gpio_output_error:
        gpio_free(keyinput->led.led_gpio);
    request_gpio_error:
    get_gpio_error:
    return ret;
}

static int __init keyinput_init(void)
{
    int i = KEY_COUNT - 1;
    int ret = 0;
    keyinput = (struct keyinput_dev *)kmalloc(sizeof(struct keyinput_dev),GFP_KERNEL);
    if(!keyinput)
    {
        printk("kmalloc error!\n");
        return -EINVAL;
    }

    init_timer(&keyinput->timer.timer);
    keyinput->timer.timer.function = timer_handle;

    keyinput->inputdev = input_allocate_device();
    if(!keyinput->inputdev)
    {
        printk("input_allocate_device error!\n");
        ret = -EINVAL;
        goto allocate_input_device_error;
    }
    keyinput->inputdev->name = KEYINPUT_NAME;
    __set_bit(EV_KEY,keyinput->inputdev->evbit);
    __set_bit(EV_REP,keyinput->inputdev->evbit);
    __set_bit(BTN_0,keyinput->inputdev->keybit);
    ret = input_register_device(keyinput->inputdev);
    if(ret)
    {
        printk("input_register_device error!\n");
        goto register_input_device_error;
    }

    ret = keyio_init();
    if(ret)
    {
        printk("key_init error!\n");
        goto key_init_error;
    }

    ret = led_init();
    if(ret)
    {
        printk("led_init error!\n");
        goto led_init_error;
    }
    printk("keyinput_init success!\n");
    return 0;

    led_init_error:
        for(; i >= 0; i--)
        {
            free_irq(keyinput->key[i].irq_num,keyinput);
            gpio_free(keyinput->key[i].key_gpio);
        }
    key_init_error:
        input_unregister_device(keyinput->inputdev);
    register_input_device_error:
        input_free_device(keyinput->inputdev);
    allocate_input_device_error:
        del_timer_sync(&keyinput->timer.timer);
        kfree(keyinput);
    return ret;
}
static void __exit keyinput_exit(void)
{
    int i = KEY_COUNT - 1;
    gpio_set_value(keyinput->led.led_gpio,1);
    gpio_free(keyinput->led.led_gpio);
    for(; i >= 0; i--)
    {
        free_irq(keyinput->key[i].irq_num,keyinput);
        gpio_free(keyinput->key[i].key_gpio);
    }
    input_unregister_device(keyinput->inputdev);
    input_free_device(keyinput->inputdev);
    del_timer_sync(&keyinput->timer.timer);
    kfree(keyinput);
    printk("keyinput_exit success!\n");
}

module_init(keyinput_init);
module_exit(keyinput_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("dong");