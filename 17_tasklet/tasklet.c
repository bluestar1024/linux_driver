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

#define  TASKLET_COUNT         1
#define  TASKLET_NAME          "tasklet"

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
    struct tasklet_struct tasklet;
    void (*key_tasklet_name)(unsigned long);
};
struct led_desc{
    int led_gpio;
    char name[10];
};
struct timer_desc{
    struct timer_list timer;
};
struct tasklet_dev{
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
struct tasklet_dev *tasklet;

irqreturn_t irq_handle(int irq_num, void *dev_id)
{
    struct tasklet_dev *dev = (struct tasklet_dev *)dev_id;
    tasklet_schedule(&dev->key[0].tasklet);
    return IRQ_HANDLED;
}
void key_tasklet(unsigned long data)
{
    struct tasklet_dev *dev = (struct tasklet_dev *)data;
    dev->timer.timer.data = data;
    mod_timer(&dev->timer.timer,jiffies + msecs_to_jiffies(20));
}
void timer_handle(unsigned long arg)
{
    struct tasklet_dev *dev = (struct tasklet_dev *)arg;
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
}

int tasklet_open(struct inode *pinode, struct file *filp)
{
    filp->private_data = tasklet;
    return 0;
}
int tasklet_close(struct inode *pinode, struct file *filp)
{
    return 0;
}

const struct file_operations tasklet_fops = {
    .owner = THIS_MODULE,
    .open = tasklet_open,
    .release = tasklet_close
};

int key_init(void)
{
    int i = 0, j = 0;
    int ret = 0;

    tasklet->dn = of_find_node_by_path(KEY_NODE);
    if(!tasklet->dn)
    {
        printk("key:of_find_node_by_path error!\n");
        return -EINVAL;
    }

    tasklet->key[0].irq_handler_name = irq_handle;
    tasklet->key[i].key_tasklet_name = key_tasklet;

    for(; i < KEY_COUNT; i++)
    {
        tasklet->key[i].key_gpio = of_get_named_gpio(tasklet->dn,KEY_PROP,i);
        if(tasklet->key[i].key_gpio < 0)
        {
            printk("key:of_get_named_gpio error!\n");
            ret = tasklet->key[i].key_gpio;
            goto get_gpio_error;
        }

        memset(tasklet->key[i].name,0,sizeof(tasklet->key[i].name));
        sprintf(tasklet->key[i].name,"GPIO_KEY%d",i);
        ret = gpio_request(tasklet->key[i].key_gpio,tasklet->key[i].name);
        if(ret)
        {
            printk("key:gpio_request error!\n");
            goto request_gpio_error;
        }

        ret = gpio_direction_input(tasklet->key[i].key_gpio);
        if(ret)
        {
            printk("key:gpio_direction_input error!\n");
            goto gpio_input_error;
        }

    #if 0
        tasklet->key[i].irq_num = gpio_to_irq(tasklet->key[i].key_gpio);
    #endif
    #if 1
        tasklet->key[i].irq_num = irq_of_parse_and_map(tasklet->dn,i);
    #endif

        memset(tasklet->key[i].name,0,sizeof(tasklet->key[i].name));
        sprintf(tasklet->key[i].name,"IRQ_KEY%d",i);
        ret = request_irq(tasklet->key[i].irq_num,tasklet->key[i].irq_handler_name,IRQF_TRIGGER_RISING|IRQF_TRIGGER_FALLING,tasklet->key[i].name,tasklet);
        if(ret)
        {
            printk("request_irq error!\n");
            goto request_irq_error;
        }

        tasklet_init(&tasklet->key[i].tasklet,tasklet->key[i].key_tasklet_name,(unsigned long)tasklet);
    }
    return 0;

    request_irq_error:
        j = i;
        for(j = j - 1; j >= 0; j--)
            free_irq(tasklet->key[j].irq_num,tasklet);
    gpio_input_error:
        gpio_free(tasklet->key[i].key_gpio);
    request_gpio_error:
        for(i = i - 1; i >= 0; i--)
            gpio_free(tasklet->key[i].key_gpio);
    get_gpio_error:
    return ret;
}
int led_init(void)
{
    int ret = 0;
    tasklet->dn = of_find_node_by_path(LED_NODE);
    if(!tasklet->dn)
    {
        printk("led:of_find_node_by_path error!\n");
        return -EINVAL;
    }

    tasklet->led.led_gpio = of_get_named_gpio(tasklet->dn,LED_PROP,0);
    if(tasklet->led.led_gpio < 0)
    {
        printk("led:of_get_named_gpio error!\n");
        ret = tasklet->led.led_gpio;
        goto get_gpio_error;
    }

    memset(tasklet->led.name,0,sizeof(tasklet->led.name));
    sprintf(tasklet->led.name,"GPIO_LED");
    ret = gpio_request(tasklet->led.led_gpio,tasklet->led.name);
    if(ret)
    {
        printk("led:gpio_request error!\n");
        goto request_gpio_error;
    }
    ret = gpio_direction_output(tasklet->led.led_gpio,1);
    if(ret)
    {
        printk("led:gpio_direction_output error!\n");
        goto gpio_output_error;
    }
    gpio_set_value(tasklet->led.led_gpio,0);
    return 0;

    gpio_output_error:
        gpio_free(tasklet->led.led_gpio);
    request_gpio_error:
    get_gpio_error:
    return ret;
}

static int __init tasklet_xx_init(void)
{
    int i = KEY_COUNT - 1;
    int ret = 0;
    tasklet = (struct tasklet_dev *)kmalloc(sizeof(struct tasklet_dev),GFP_KERNEL);
    if(!tasklet)
    {
        printk("kmalloc error!\n");
        return -EINVAL;
    }
    init_timer(&tasklet->timer.timer);
    tasklet->timer.timer.function = timer_handle;

    tasklet->major = 0;
    tasklet->minor = 0;
    if(tasklet->major)
    {
        tasklet->devid = MKDEV(tasklet->major,tasklet->minor);
        ret = register_chrdev_region(tasklet->devid,TASKLET_COUNT,TASKLET_NAME);
        if(ret < 0)
        {
            printk("register_chrdev_region error!\n");
            goto chrdev_region_error;
        }
    }else
    {
        ret = alloc_chrdev_region(&tasklet->devid,tasklet->minor,TASKLET_COUNT,TASKLET_NAME);
        if(ret < 0)
        {
            printk("alloc_chrdev_region error!\n");
            goto chrdev_region_error;
        }
        tasklet->major = MAJOR(tasklet->devid);
        tasklet->minor = MINOR(tasklet->devid);
    }
    printk("tasklet major:%d,minor:%d!\n",tasklet->major,tasklet->minor);
    tasklet->cdev.owner = THIS_MODULE;
    cdev_init(&tasklet->cdev,&tasklet_fops);
    ret = cdev_add(&tasklet->cdev,tasklet->devid,TASKLET_COUNT);
    if(ret < 0)
    {
        printk("cdev_add error!\n");
        goto cdev_add_error;
    }
    tasklet->pclass = class_create(THIS_MODULE,TASKLET_NAME);
    if(IS_ERR(tasklet->pclass))
    {
        printk("class_create error!\n");
        ret = PTR_ERR(tasklet->pclass);
        goto class_create_error;
    }
    tasklet->pdevice = device_create(tasklet->pclass,NULL,tasklet->devid,NULL,TASKLET_NAME);
    if(IS_ERR(tasklet->pdevice))
    {
        printk("device_create error!\n");
        ret = PTR_ERR(tasklet->pdevice);
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
    printk("tasklet_init success!\n");
    return 0;

    led_init_error:
        for(; i >= 0; i--)
        {
            free_irq(tasklet->key[i].irq_num,tasklet);
            gpio_free(tasklet->key[i].key_gpio);
        }
    key_init_error:
        device_destroy(tasklet->pclass,tasklet->devid);
    device_create_error:
        class_destroy(tasklet->pclass);
    class_create_error:
        cdev_del(&tasklet->cdev);
    cdev_add_error:
        unregister_chrdev_region(tasklet->devid,TASKLET_COUNT);
    chrdev_region_error:
        del_timer_sync(&tasklet->timer.timer);
        kfree(tasklet);
    return ret;
}
static void __exit tasklet_xx_exit(void)
{
    int i = KEY_COUNT - 1;
    gpio_set_value(tasklet->led.led_gpio,1);
    gpio_free(tasklet->led.led_gpio);
    for(; i >= 0; i--)
    {
        free_irq(tasklet->key[i].irq_num,tasklet);
        gpio_free(tasklet->key[i].key_gpio);
    }
    device_destroy(tasklet->pclass,tasklet->devid);
    class_destroy(tasklet->pclass);
    cdev_del(&tasklet->cdev);
    unregister_chrdev_region(tasklet->devid,TASKLET_COUNT);
    del_timer_sync(&tasklet->timer.timer);
    kfree(tasklet);
    printk("tasklet_exit success!\n");
}

module_init(tasklet_xx_init);
module_exit(tasklet_xx_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("dong");