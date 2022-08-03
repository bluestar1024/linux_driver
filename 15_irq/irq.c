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

#define  IRQ_COUNT          1
#define  IRQ_NAME           "irq"

#define  KEY_COUNT          1

#define  KEY_NODE           "/key"
#define  KEY_PROP           "key-gpio"

#define  LED_NODE           "/gpioled"
#define  LED_PROP           "led-gpio"

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
struct irq_dev{
    dev_t devid;
    int major;
    int minor;
    struct cdev cdev;
    struct class *pclass;
    struct device *pdevice;
    struct device_node *dn;
    struct irq_keydesc key[KEY_COUNT];
    struct led_desc led;
};
struct irq_dev *irq;

irqreturn_t irq_handler(int irq_num, void *dev_id)
{
    struct irq_dev *dev = (struct irq_dev *)dev_id;
    if(gpio_get_value(dev->key[0].key_gpio))
    {
        printk("KEY0 release!\n");
        gpio_set_value(dev->led.led_gpio,0);
    }
    else
    {
        printk("KEY0 press!\n");
        gpio_set_value(dev->led.led_gpio,1);
    }
    return IRQ_HANDLED;
}

int irq_open(struct inode *pinode, struct file *filp)
{
    filp->private_data = irq;
    return 0;
}
int irq_close(struct inode *pinode, struct file *filp)
{
    return 0;
}

const struct file_operations irq_fops = {
    .owner = THIS_MODULE,
    .open = irq_open,
    .release = irq_close
};

int key_init(void)
{
    int i = 0, j = 0;
    int ret = 0;

    irq->dn = of_find_node_by_path(KEY_NODE);
    if(!irq->dn)
    {
        printk("key:of_find_node_by_path error!\n");
        return -EINVAL;
    }

    irq->key[0].irq_handler_name = irq_handler;

    for(; i < KEY_COUNT; i++)
    {
        irq->key[i].key_gpio = of_get_named_gpio(irq->dn,KEY_PROP,i);
        if(irq->key[i].key_gpio < 0)
        {
            printk("key:of_get_named_gpio error!\n");
            ret = irq->key[i].key_gpio;
            goto get_gpio_error;
        }

        memset(irq->key[i].name,0,sizeof(irq->key[i].name));
        sprintf(irq->key[i].name,"GPIO_KEY%d",i);
        ret = gpio_request(irq->key[i].key_gpio,irq->key[i].name);
        if(ret)
        {
            printk("key:gpio_request error!\n");
            goto request_gpio_error;
        }

        ret = gpio_direction_input(irq->key[i].key_gpio);
        if(ret)
        {
            printk("key:gpio_direction_input error!\n");
            goto gpio_input_error;
        }

    #if 0
        irq->key[i].irq_num = gpio_to_irq(irq->key[i].key_gpio);
    #endif
    #if 1
        irq->key[i].irq_num = irq_of_parse_and_map(irq->dn,i);
    #endif

        memset(irq->key[i].name,0,sizeof(irq->key[i].name));
        sprintf(irq->key[i].name,"IRQ_KEY%d",i);
        ret = request_irq(irq->key[i].irq_num,irq->key[i].irq_handler_name,IRQF_TRIGGER_RISING|IRQF_TRIGGER_FALLING,irq->key[i].name,irq);
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
            free_irq(irq->key[j].irq_num,irq);
    gpio_input_error:
        gpio_free(irq->key[i].key_gpio);
    request_gpio_error:
        for(i = i - 1; i >= 0; i--)
            gpio_free(irq->key[i].key_gpio);
    get_gpio_error:
    return ret;
}
int led_init(void)
{
    int ret = 0;
    irq->dn = of_find_node_by_path(LED_NODE);
    if(!irq->dn)
    {
        printk("led:of_find_node_by_path error!\n");
        return -EINVAL;
    }

    irq->led.led_gpio = of_get_named_gpio(irq->dn,LED_PROP,0);
    if(irq->led.led_gpio < 0)
    {
        printk("led:of_get_named_gpio error!\n");
        ret = irq->led.led_gpio;
        goto get_gpio_error;
    }

    memset(irq->led.name,0,sizeof(irq->led.name));
    sprintf(irq->led.name,"GPIO_LED");
    ret = gpio_request(irq->led.led_gpio,irq->led.name);
    if(ret)
    {
        printk("led:gpio_request error!\n");
        goto request_gpio_error;
    }
    ret = gpio_direction_output(irq->led.led_gpio,1);
    if(ret)
    {
        printk("led:gpio_direction_output error!\n");
        goto gpio_output_error;
    }
    gpio_set_value(irq->led.led_gpio,0);
    return 0;

    gpio_output_error:
        gpio_free(irq->led.led_gpio);
    request_gpio_error:
    get_gpio_error:
    return ret;
}

static int __init irq_xx_init(void)
{
    int i = KEY_COUNT - 1;
    int ret = 0;
    irq = (struct irq_dev *)kmalloc(sizeof(struct irq_dev),GFP_KERNEL);
    if(!irq)
    {
        printk("kmalloc error!\n");
        return -EINVAL;
    }

    irq->major = 0;
    irq->minor = 0;
    if(irq->major)
    {
        irq->devid = MKDEV(irq->major,irq->minor);
        ret = register_chrdev_region(irq->devid,IRQ_COUNT,IRQ_NAME);
        if(ret < 0)
        {
            printk("register_chrdev_region error!\n");
            goto chrdev_region_error;
        }
    }else
    {
        ret = alloc_chrdev_region(&irq->devid,irq->minor,IRQ_COUNT,IRQ_NAME);
        if(ret < 0)
        {
            printk("alloc_chrdev_region error!\n");
            goto chrdev_region_error;
        }
        irq->major = MAJOR(irq->devid);
        irq->minor = MINOR(irq->devid);
    }
    printk("irq major:%d,minor:%d!\n",irq->major,irq->minor);
    irq->cdev.owner = THIS_MODULE;
    cdev_init(&irq->cdev,&irq_fops);
    ret = cdev_add(&irq->cdev,irq->devid,IRQ_COUNT);
    if(ret < 0)
    {
        printk("cdev_add error!\n");
        goto cdev_add_error;
    }
    irq->pclass = class_create(THIS_MODULE,IRQ_NAME);
    if(IS_ERR(irq->pclass))
    {
        printk("class_create error!\n");
        ret = PTR_ERR(irq->pclass);
        goto class_create_error;
    }
    irq->pdevice = device_create(irq->pclass,NULL,irq->devid,NULL,IRQ_NAME);
    if(IS_ERR(irq->pdevice))
    {
        printk("device_create error!\n");
        ret = PTR_ERR(irq->pdevice);
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
    printk("irq_xx_init success!\n");
    return 0;

    led_init_error:
        for(; i >= 0; i--)
        {
            free_irq(irq->key[i].irq_num,irq);
            gpio_free(irq->key[i].key_gpio);
        }
    key_init_error:
        device_destroy(irq->pclass,irq->devid);
    device_create_error:
        class_destroy(irq->pclass);
    class_create_error:
        cdev_del(&irq->cdev);
    cdev_add_error:
        unregister_chrdev_region(irq->devid,IRQ_COUNT);
    chrdev_region_error:
        kfree(irq);
    return ret;
}
static void __exit irq_xx_exit(void)
{
    int i = KEY_COUNT - 1;
    gpio_set_value(irq->led.led_gpio,1);
    gpio_free(irq->led.led_gpio);
    for(; i >= 0; i--)
    {
        free_irq(irq->key[i].irq_num,irq);
        gpio_free(irq->key[i].key_gpio);
    }
    device_destroy(irq->pclass,irq->devid);
    class_destroy(irq->pclass);
    cdev_del(&irq->cdev);
    unregister_chrdev_region(irq->devid,IRQ_COUNT);
    kfree(irq);
    printk("irq_xx_exit success!\n");
}

module_init(irq_xx_init);
module_exit(irq_xx_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("dong");