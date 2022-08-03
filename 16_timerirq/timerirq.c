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

#define  TIMERIRQ_COUNT         1
#define  TIMERIRQ_NAME          "timerirq"

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
};
struct led_desc{
    int led_gpio;
    char name[10];
};
struct timer_desc{
    struct timer_list timer;
};
struct timerirq_dev{
    dev_t devid;
    int major;
    int minor;
    struct cdev cdev;
    struct class *pclass;
    struct device *pdevice;
    struct device_node *dn;
    struct irq_keydesc key[KEY_COUNT];
    struct led_desc led;
    struct timer_desc timer;
};
struct timerirq_dev *timerirq;

irqreturn_t irq_handle(int irq_num, void *dev_id)
{
    struct timerirq_dev *dev = (struct timerirq_dev *)dev_id;
    dev->timer.timer.data = gpio_get_value(dev->key[0].key_gpio);
    mod_timer(&dev->timer.timer,jiffies + msecs_to_jiffies(20));
    //disable_irq_nosync(dev->key[0].irq_num);
    return IRQ_HANDLED;
}
void timer_handle(unsigned long arg)
{
    //if(arg == gpio_get_value(timerirq->key[0].key_gpio))
    //{
        if(gpio_get_value(timerirq->key[0].key_gpio))
        {
            printk("KEY0 release!\n");
            gpio_set_value(timerirq->led.led_gpio,0);
        }
        else
        {
            printk("KEY0 press!\n");
            gpio_set_value(timerirq->led.led_gpio,1);
        }
    //}
    //enable_irq(timerirq->key[0].irq_num);
}

int timerirq_open(struct inode *pinode, struct file *filp)
{
    filp->private_data = timerirq;
    return 0;
}
int timerirq_close(struct inode *pinode, struct file *filp)
{
    return 0;
}

const struct file_operations timerirq_fops = {
    .owner = THIS_MODULE,
    .open = timerirq_open,
    .release = timerirq_close
};

int key_init(void)
{
    int i = 0, j = 0;
    int ret = 0;

    timerirq->dn = of_find_node_by_path(KEY_NODE);
    if(!timerirq->dn)
    {
        printk("key:of_find_node_by_path error!\n");
        return -EINVAL;
    }

    timerirq->key[0].irq_handler_name = irq_handle;

    for(; i < KEY_COUNT; i++)
    {
        timerirq->key[i].key_gpio = of_get_named_gpio(timerirq->dn,KEY_PROP,i);
        if(timerirq->key[i].key_gpio < 0)
        {
            printk("key:of_get_named_gpio error!\n");
            ret = timerirq->key[i].key_gpio;
            goto get_gpio_error;
        }

        memset(timerirq->key[i].name,0,sizeof(timerirq->key[i].name));
        sprintf(timerirq->key[i].name,"GPIO_KEY%d",i);
        ret = gpio_request(timerirq->key[i].key_gpio,timerirq->key[i].name);
        if(ret)
        {
            printk("key:gpio_request error!\n");
            goto request_gpio_error;
        }

        ret = gpio_direction_input(timerirq->key[i].key_gpio);
        if(ret)
        {
            printk("key:gpio_direction_input error!\n");
            goto gpio_input_error;
        }

    #if 0
        timerirq->key[i].irq_num = gpio_to_irq(timerirq->key[i].key_gpio);
    #endif
    #if 1
        timerirq->key[i].irq_num = irq_of_parse_and_map(timerirq->dn,i);
    #endif

        memset(timerirq->key[i].name,0,sizeof(timerirq->key[i].name));
        sprintf(timerirq->key[i].name,"IRQ_KEY%d",i);
        ret = request_irq(timerirq->key[i].irq_num,timerirq->key[i].irq_handler_name,IRQF_TRIGGER_RISING|IRQF_TRIGGER_FALLING,timerirq->key[i].name,timerirq);
        if(ret)
        {
            printk("request_irq error!\n");
            goto request_irq_error;
        }
    }
    return 0;

    request_irq_error:
        j = i;
        for(j = j - 1; j >= 0; j--)
            free_irq(timerirq->key[j].irq_num,timerirq);
    gpio_input_error:
        gpio_free(timerirq->key[i].key_gpio);
    request_gpio_error:
        for(i = i - 1; i >= 0; i--)
            gpio_free(timerirq->key[i].key_gpio);
    get_gpio_error:
    return ret;
}
int led_init(void)
{
    int ret = 0;
    timerirq->dn = of_find_node_by_path(LED_NODE);
    if(!timerirq->dn)
    {
        printk("led:of_find_node_by_path error!\n");
        return -EINVAL;
    }

    timerirq->led.led_gpio = of_get_named_gpio(timerirq->dn,LED_PROP,0);
    if(timerirq->led.led_gpio < 0)
    {
        printk("led:of_get_named_gpio error!\n");
        ret = timerirq->led.led_gpio;
        goto get_gpio_error;
    }

    memset(timerirq->led.name,0,sizeof(timerirq->led.name));
    sprintf(timerirq->led.name,"GPIO_LED");
    ret = gpio_request(timerirq->led.led_gpio,timerirq->led.name);
    if(ret)
    {
        printk("led:gpio_request error!\n");
        goto request_gpio_error;
    }
    ret = gpio_direction_output(timerirq->led.led_gpio,1);
    if(ret)
    {
        printk("led:gpio_direction_output error!\n");
        goto gpio_output_error;
    }
    gpio_set_value(timerirq->led.led_gpio,0);
    return 0;

    gpio_output_error:
        gpio_free(timerirq->led.led_gpio);
    request_gpio_error:
    get_gpio_error:
    return ret;
}

static int __init timerirq_init(void)
{
    int i = KEY_COUNT - 1;
    int ret = 0;
    timerirq = (struct timerirq_dev *)kmalloc(sizeof(struct timerirq_dev),GFP_KERNEL);
    if(!timerirq)
    {
        printk("kmalloc error!\n");
        return -EINVAL;
    }
    init_timer(&timerirq->timer.timer);
    timerirq->timer.timer.function = timer_handle;

    timerirq->major = 0;
    timerirq->minor = 0;
    if(timerirq->major)
    {
        timerirq->devid = MKDEV(timerirq->major,timerirq->minor);
        ret = register_chrdev_region(timerirq->devid,TIMERIRQ_COUNT,TIMERIRQ_NAME);
        if(ret < 0)
        {
            printk("register_chrdev_region error!\n");
            goto chrdev_region_error;
        }
    }else
    {
        ret = alloc_chrdev_region(&timerirq->devid,timerirq->minor,TIMERIRQ_COUNT,TIMERIRQ_NAME);
        if(ret < 0)
        {
            printk("alloc_chrdev_region error!\n");
            goto chrdev_region_error;
        }
        timerirq->major = MAJOR(timerirq->devid);
        timerirq->minor = MINOR(timerirq->devid);
    }
    printk("timerirq major:%d,minor:%d!\n",timerirq->major,timerirq->minor);
    timerirq->cdev.owner = THIS_MODULE;
    cdev_init(&timerirq->cdev,&timerirq_fops);
    ret = cdev_add(&timerirq->cdev,timerirq->devid,TIMERIRQ_COUNT);
    if(ret < 0)
    {
        printk("cdev_add error!\n");
        goto cdev_add_error;
    }
    timerirq->pclass = class_create(THIS_MODULE,TIMERIRQ_NAME);
    if(IS_ERR(timerirq->pclass))
    {
        printk("class_create error!\n");
        ret = PTR_ERR(timerirq->pclass);
        goto class_create_error;
    }
    timerirq->pdevice = device_create(timerirq->pclass,NULL,timerirq->devid,NULL,TIMERIRQ_NAME);
    if(IS_ERR(timerirq->pdevice))
    {
        printk("device_create error!\n");
        ret = PTR_ERR(timerirq->pdevice);
        goto device_create_error;
    }

    ret = key_init();
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
    printk("timerirq_init success!\n");
    return 0;

    led_init_error:
        for(; i >= 0; i--)
        {
            free_irq(timerirq->key[i].irq_num,timerirq);
            gpio_free(timerirq->key[i].key_gpio);
        }
    key_init_error:
        device_destroy(timerirq->pclass,timerirq->devid);
    device_create_error:
        class_destroy(timerirq->pclass);
    class_create_error:
        cdev_del(&timerirq->cdev);
    cdev_add_error:
        unregister_chrdev_region(timerirq->devid,TIMERIRQ_COUNT);
    chrdev_region_error:
        del_timer_sync(&timerirq->timer.timer);
        kfree(timerirq);
    return ret;
}
static void __exit timerirq_exit(void)
{
    int i = KEY_COUNT - 1;
    gpio_set_value(timerirq->led.led_gpio,1);
    gpio_free(timerirq->led.led_gpio);
    for(; i >= 0; i--)
    {
        free_irq(timerirq->key[i].irq_num,timerirq);
        gpio_free(timerirq->key[i].key_gpio);
    }
    device_destroy(timerirq->pclass,timerirq->devid);
    class_destroy(timerirq->pclass);
    cdev_del(&timerirq->cdev);
    unregister_chrdev_region(timerirq->devid,TIMERIRQ_COUNT);
    del_timer_sync(&timerirq->timer.timer);
    kfree(timerirq);
    printk("timerirq_exit success!\n");
}

module_init(timerirq_init);
module_exit(timerirq_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("dong");