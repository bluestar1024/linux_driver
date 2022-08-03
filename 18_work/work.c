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

#define  WORK_COUNT         1
#define  WORK_NAME          "work"

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
struct work_dev{
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
struct work_dev *work;

irqreturn_t irq_handle(int irq_num, void *dev_id)
{
    struct work_dev *dev = (struct work_dev *)dev_id;
    //tasklet_schedule(&dev->key[0].tasklet);
    schedule_work(&dev->key[0].work);
    return IRQ_HANDLED;
}
#if 0
void key_tasklet(unsigned long data)
{
    struct work_dev *dev = (struct work_dev *)data;
    dev->timer.timer.data = data;
    mod_timer(&dev->timer.timer,jiffies + msecs_to_jiffies(20));
}
#endif
void key_work(struct work_struct *work)
{
    struct irq_keydesc *key0_struct_ptr = container_of(work,struct irq_keydesc,work);
    struct work_dev *dev = container_of(key0_struct_ptr,struct work_dev,key[0]);
    dev->timer.timer.data = (unsigned long)dev;
    mod_timer(&dev->timer.timer,jiffies + msecs_to_jiffies(20));
}
void timer_handle(unsigned long arg)
{
    struct work_dev *dev = (struct work_dev *)arg;
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

int work_open(struct inode *pinode, struct file *filp)
{
    filp->private_data = work;
    return 0;
}
int work_close(struct inode *pinode, struct file *filp)
{
    return 0;
}

const struct file_operations work_fops = {
    .owner = THIS_MODULE,
    .open = work_open,
    .release = work_close
};

int key_init(void)
{
    int i = 0, j = 0;
    int ret = 0;

    work->dn = of_find_node_by_path(KEY_NODE);
    if(!work->dn)
    {
        printk("key:of_find_node_by_path error!\n");
        return -EINVAL;
    }

    work->key[0].irq_handler_name = irq_handle;
    //work->key[0].key_tasklet_name = key_tasklet;
    work->key[0].key_work_name = key_work;

    for(; i < KEY_COUNT; i++)
    {
        work->key[i].key_gpio = of_get_named_gpio(work->dn,KEY_PROP,i);
        if(work->key[i].key_gpio < 0)
        {
            printk("key:of_get_named_gpio error!\n");
            ret = work->key[i].key_gpio;
            goto get_gpio_error;
        }

        memset(work->key[i].name,0,sizeof(work->key[i].name));
        sprintf(work->key[i].name,"GPIO_KEY%d",i);
        ret = gpio_request(work->key[i].key_gpio,work->key[i].name);
        if(ret)
        {
            printk("key:gpio_request error!\n");
            goto request_gpio_error;
        }

        ret = gpio_direction_input(work->key[i].key_gpio);
        if(ret)
        {
            printk("key:gpio_direction_input error!\n");
            goto gpio_input_error;
        }

    #if 0
        work->key[i].irq_num = gpio_to_irq(work->key[i].key_gpio);
    #endif
    #if 1
        work->key[i].irq_num = irq_of_parse_and_map(work->dn,i);
    #endif

        memset(work->key[i].name,0,sizeof(work->key[i].name));
        sprintf(work->key[i].name,"IRQ_KEY%d",i);
        ret = request_irq(work->key[i].irq_num,work->key[i].irq_handler_name,IRQF_TRIGGER_RISING|IRQF_TRIGGER_FALLING,work->key[i].name,work);
        if(ret)
        {
            printk("request_irq error!\n");
            goto request_irq_error;
        }

        //tasklet_init(&work->key[i].tasklet,work->key[i].key_tasklet_name,(unsigned long)work);
        INIT_WORK(&work->key[i].work,work->key[i].key_work_name);
    }
    return 0;

    request_irq_error:
        j = i;
        for(j = j - 1; j >= 0; j--)
            free_irq(work->key[j].irq_num,work);
    gpio_input_error:
        gpio_free(work->key[i].key_gpio);
    request_gpio_error:
        for(i = i - 1; i >= 0; i--)
            gpio_free(work->key[i].key_gpio);
    get_gpio_error:
    return ret;
}
int led_init(void)
{
    int ret = 0;
    work->dn = of_find_node_by_path(LED_NODE);
    if(!work->dn)
    {
        printk("led:of_find_node_by_path error!\n");
        return -EINVAL;
    }

    work->led.led_gpio = of_get_named_gpio(work->dn,LED_PROP,0);
    if(work->led.led_gpio < 0)
    {
        printk("led:of_get_named_gpio error!\n");
        ret = work->led.led_gpio;
        goto get_gpio_error;
    }

    memset(work->led.name,0,sizeof(work->led.name));
    sprintf(work->led.name,"GPIO_LED");
    ret = gpio_request(work->led.led_gpio,work->led.name);
    if(ret)
    {
        printk("led:gpio_request error!\n");
        goto request_gpio_error;
    }
    ret = gpio_direction_output(work->led.led_gpio,1);
    if(ret)
    {
        printk("led:gpio_direction_output error!\n");
        goto gpio_output_error;
    }
    gpio_set_value(work->led.led_gpio,0);
    return 0;

    gpio_output_error:
        gpio_free(work->led.led_gpio);
    request_gpio_error:
    get_gpio_error:
    return ret;
}

static int __init work_init(void)
{
    int i = KEY_COUNT - 1;
    int ret = 0;
    work = (struct work_dev *)kmalloc(sizeof(struct work_dev),GFP_KERNEL);
    if(!work)
    {
        printk("kmalloc error!\n");
        return -EINVAL;
    }
    init_timer(&work->timer.timer);
    work->timer.timer.function = timer_handle;

    work->major = 0;
    work->minor = 0;
    if(work->major)
    {
        work->devid = MKDEV(work->major,work->minor);
        ret = register_chrdev_region(work->devid,WORK_COUNT,WORK_NAME);
        if(ret < 0)
        {
            printk("register_chrdev_region error!\n");
            goto chrdev_region_error;
        }
    }else
    {
        ret = alloc_chrdev_region(&work->devid,work->minor,WORK_COUNT,WORK_NAME);
        if(ret < 0)
        {
            printk("alloc_chrdev_region error!\n");
            goto chrdev_region_error;
        }
        work->major = MAJOR(work->devid);
        work->minor = MINOR(work->devid);
    }
    printk("work major:%d,minor:%d!\n",work->major,work->minor);
    work->cdev.owner = THIS_MODULE;
    cdev_init(&work->cdev,&work_fops);
    ret = cdev_add(&work->cdev,work->devid,WORK_COUNT);
    if(ret < 0)
    {
        printk("cdev_add error!\n");
        goto cdev_add_error;
    }
    work->pclass = class_create(THIS_MODULE,WORK_NAME);
    if(IS_ERR(work->pclass))
    {
        printk("class_create error!\n");
        ret = PTR_ERR(work->pclass);
        goto class_create_error;
    }
    work->pdevice = device_create(work->pclass,NULL,work->devid,NULL,WORK_NAME);
    if(IS_ERR(work->pdevice))
    {
        printk("device_create error!\n");
        ret = PTR_ERR(work->pdevice);
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
    printk("work_init success!\n");
    return 0;

    led_init_error:
        for(; i >= 0; i--)
        {
            free_irq(work->key[i].irq_num,work);
            gpio_free(work->key[i].key_gpio);
        }
    key_init_error:
        device_destroy(work->pclass,work->devid);
    device_create_error:
        class_destroy(work->pclass);
    class_create_error:
        cdev_del(&work->cdev);
    cdev_add_error:
        unregister_chrdev_region(work->devid,WORK_COUNT);
    chrdev_region_error:
        del_timer_sync(&work->timer.timer);
        kfree(work);
    return ret;
}
static void __exit work_exit(void)
{
    int i = KEY_COUNT - 1;
    gpio_set_value(work->led.led_gpio,1);
    gpio_free(work->led.led_gpio);
    for(; i >= 0; i--)
    {
        free_irq(work->key[i].irq_num,work);
        gpio_free(work->key[i].key_gpio);
    }
    device_destroy(work->pclass,work->devid);
    class_destroy(work->pclass);
    cdev_del(&work->cdev);
    unregister_chrdev_region(work->devid,WORK_COUNT);
    del_timer_sync(&work->timer.timer);
    kfree(work);
    printk("work_exit success!\n");
}

module_init(work_init);
module_exit(work_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("dong");