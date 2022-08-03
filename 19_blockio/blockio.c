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

#define  BLOCKIO_COUNT         1
#define  BLOCKIO_NAME          "blockio"

#define  KEY_COUNT              1

#define  KEY_NODE               "/key"
#define  KEY_PROP               "key-gpio"

#define  LED_NODE               "/gpioled"
#define  LED_PROP               "led-gpio"

#define  KEY0VALUE              0x01
#define  INVALUE                0xff

struct irq_keydesc{
    int key_gpio;
    unsigned int irq_num;
    irq_handler_t irq_handler_name;
    char name[10];
    unsigned char value;

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
struct blockio_dev{
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

    atomic_t keyvalue;
    atomic_t releasekey;
    unsigned char curkeynum;

    wait_queue_head_t read_wait_head;
};
struct blockio_dev *blockio;

irqreturn_t irq_handle(int irq_num, void *dev_id)
{
    struct blockio_dev *dev = (struct blockio_dev *)dev_id;
    //tasklet_schedule(&dev->key[0].tasklet);
    schedule_work(&dev->key[0].work);
    return IRQ_HANDLED;
}
#if 0
void key_tasklet(unsigned long data)
{
    struct blockio_dev *dev = (struct blockio_dev *)data;
    dev->timer.timer.data = data;
    mod_timer(&dev->timer.timer,jiffies + msecs_to_jiffies(20));
}
#endif
void key_work(struct work_struct *work)
{
    struct irq_keydesc *key0_struct_ptr = container_of(work,struct irq_keydesc,work);
    struct blockio_dev *dev = container_of(key0_struct_ptr,struct blockio_dev,key[0]);
    dev->curkeynum = 0;
    dev->timer.timer.data = (unsigned long)dev;
    mod_timer(&dev->timer.timer,jiffies + msecs_to_jiffies(20));
}
void timer_handle(unsigned long arg)
{
    unsigned char num = 0;
    unsigned char value = 0;
    struct blockio_dev *dev = (struct blockio_dev *)arg;
    num = dev->curkeynum;
    value = gpio_get_value(dev->key[num].key_gpio);
    if(value == 0)
    {
        //printk("KEY0 press!\n");
        gpio_set_value(dev->led.led_gpio,1);

        atomic_set(&dev->keyvalue,KEY0VALUE);
    }
    else
    {
        //printk("KEY0 release!\n");
        gpio_set_value(dev->led.led_gpio,0);

        atomic_set(&dev->keyvalue,KEY0VALUE|0x80);
        atomic_set(&dev->releasekey,1);
    }
    if(atomic_read(&dev->releasekey)){
        wake_up(&dev->read_wait_head);
    }
}

int blockio_open(struct inode *pinode, struct file *filp)
{
    filp->private_data = blockio;
    return 0;
}
ssize_t blockio_read(struct file *filp, char __user *pbuf, size_t count, loff_t *ploff)
{
    int keyvalue = 0;
    int releasekey = 0;
    int ret = 0;
    struct blockio_dev *dev = (struct blockio_dev *)filp->private_data;

    //wait_event_interruptible(dev->read_wait_head,atomic_read(&dev->releasekey));

    DECLARE_WAITQUEUE(read_wait_item,current);
    add_wait_queue(&dev->read_wait_head,&read_wait_item);
    set_current_state(TASK_INTERRUPTIBLE);
    schedule();
    set_current_state(TASK_RUNNING);
    remove_wait_queue(&dev->read_wait_head,&read_wait_item);
    if(signal_pending(current)){
        goto wait_error;
    }

    keyvalue = atomic_read(&dev->keyvalue);
    releasekey = atomic_read(&dev->releasekey);
    if(releasekey)
    {
        if(keyvalue&0x80)
        {
            keyvalue &=~ 0x80;
            ret = copy_to_user(pbuf,&keyvalue,count);
            if(ret != 0){
                printk("copy error!\n");
            }
            atomic_set(&dev->releasekey,0);
        }else{
            goto data_error;
        }
    }else{
        goto data_error;
    }
    return 0;

    wait_error:
    return -ERESTARTSYS;

    data_error:
    return -EINVAL;
}
int blockio_close(struct inode *pinode, struct file *filp)
{
    return 0;
}

const struct file_operations blockio_fops = {
    .owner = THIS_MODULE,
    .open = blockio_open,
    .read = blockio_read,
    .release = blockio_close
};

int keyio_init(void)
{
    int i = 0, j = 0;
    int ret = 0;

    blockio->dn = of_find_node_by_path(KEY_NODE);
    if(!blockio->dn)
    {
        printk("key:of_find_node_by_path error!\n");
        return -EINVAL;
    }

    blockio->key[0].irq_handler_name = irq_handle;
    //blockio->key[0].key_tasklet_name = key_tasklet;
    blockio->key[0].key_work_name = key_work;

    for(; i < KEY_COUNT; i++)
    {
        blockio->key[i].key_gpio = of_get_named_gpio(blockio->dn,KEY_PROP,i);
        if(blockio->key[i].key_gpio < 0)
        {
            printk("key:of_get_named_gpio error!\n");
            ret = blockio->key[i].key_gpio;
            goto get_gpio_error;
        }

        memset(blockio->key[i].name,0,sizeof(blockio->key[i].name));
        sprintf(blockio->key[i].name,"GPIO_KEY%d",i);
        ret = gpio_request(blockio->key[i].key_gpio,blockio->key[i].name);
        if(ret)
        {
            printk("key:gpio_request error!\n");
            goto request_gpio_error;
        }

        ret = gpio_direction_input(blockio->key[i].key_gpio);
        if(ret)
        {
            printk("key:gpio_direction_input error!\n");
            goto gpio_input_error;
        }

    #if 0
        blockio->key[i].irq_num = gpio_to_irq(blockio->key[i].key_gpio);
    #endif
    #if 1
        blockio->key[i].irq_num = irq_of_parse_and_map(blockio->dn,i);
    #endif

        memset(blockio->key[i].name,0,sizeof(blockio->key[i].name));
        sprintf(blockio->key[i].name,"IRQ_KEY%d",i);
        ret = request_irq(blockio->key[i].irq_num,blockio->key[i].irq_handler_name,IRQF_TRIGGER_RISING|IRQF_TRIGGER_FALLING,blockio->key[i].name,blockio);
        if(ret)
        {
            printk("request_irq error!\n");
            goto request_irq_error;
        }

        //tasklet_init(&blockio->key[i].tasklet,blockio->key[i].key_tasklet_name,(unsigned long)blockio);
        INIT_WORK(&blockio->key[i].work,blockio->key[i].key_work_name);
    }
    return 0;

    request_irq_error:
        j = i;
        for(j = j - 1; j >= 0; j--)
            free_irq(blockio->key[j].irq_num,blockio);
    gpio_input_error:
        gpio_free(blockio->key[i].key_gpio);
    request_gpio_error:
        for(i = i - 1; i >= 0; i--)
            gpio_free(blockio->key[i].key_gpio);
    get_gpio_error:
    return ret;
}
int led_init(void)
{
    int ret = 0;
    blockio->dn = of_find_node_by_path(LED_NODE);
    if(!blockio->dn)
    {
        printk("led:of_find_node_by_path error!\n");
        return -EINVAL;
    }

    blockio->led.led_gpio = of_get_named_gpio(blockio->dn,LED_PROP,0);
    if(blockio->led.led_gpio < 0)
    {
        printk("led:of_get_named_gpio error!\n");
        ret = blockio->led.led_gpio;
        goto get_gpio_error;
    }

    memset(blockio->led.name,0,sizeof(blockio->led.name));
    sprintf(blockio->led.name,"GPIO_LED");
    ret = gpio_request(blockio->led.led_gpio,blockio->led.name);
    if(ret)
    {
        printk("led:gpio_request error!\n");
        goto request_gpio_error;
    }
    ret = gpio_direction_output(blockio->led.led_gpio,1);
    if(ret)
    {
        printk("led:gpio_direction_output error!\n");
        goto gpio_output_error;
    }
    gpio_set_value(blockio->led.led_gpio,0);
    return 0;

    gpio_output_error:
        gpio_free(blockio->led.led_gpio);
    request_gpio_error:
    get_gpio_error:
    return ret;
}

static int __init blockio_init(void)
{
    int i = KEY_COUNT - 1;
    int ret = 0;
    blockio = (struct blockio_dev *)kmalloc(sizeof(struct blockio_dev),GFP_KERNEL);
    if(!blockio)
    {
        printk("kmalloc error!\n");
        return -EINVAL;
    }
    init_timer(&blockio->timer.timer);
    blockio->timer.timer.function = timer_handle;
    atomic_set(&blockio->keyvalue,INVALUE);
    atomic_set(&blockio->releasekey,0);
    init_waitqueue_head(&blockio->read_wait_head);

    blockio->major = 0;
    blockio->minor = 0;
    if(blockio->major)
    {
        blockio->devid = MKDEV(blockio->major,blockio->minor);
        ret = register_chrdev_region(blockio->devid,BLOCKIO_COUNT,BLOCKIO_NAME);
        if(ret < 0)
        {
            printk("register_chrdev_region error!\n");
            goto chrdev_region_error;
        }
    }else
    {
        ret = alloc_chrdev_region(&blockio->devid,blockio->minor,BLOCKIO_COUNT,BLOCKIO_NAME);
        if(ret < 0)
        {
            printk("alloc_chrdev_region error!\n");
            goto chrdev_region_error;
        }
        blockio->major = MAJOR(blockio->devid);
        blockio->minor = MINOR(blockio->devid);
    }
    printk("blockio major:%d,minor:%d!\n",blockio->major,blockio->minor);
    blockio->cdev.owner = THIS_MODULE;
    cdev_init(&blockio->cdev,&blockio_fops);
    ret = cdev_add(&blockio->cdev,blockio->devid,BLOCKIO_COUNT);
    if(ret < 0)
    {
        printk("cdev_add error!\n");
        goto cdev_add_error;
    }
    blockio->pclass = class_create(THIS_MODULE,BLOCKIO_NAME);
    if(IS_ERR(blockio->pclass))
    {
        printk("class_create error!\n");
        ret = PTR_ERR(blockio->pclass);
        goto class_create_error;
    }
    blockio->pdevice = device_create(blockio->pclass,NULL,blockio->devid,NULL,BLOCKIO_NAME);
    if(IS_ERR(blockio->pdevice))
    {
        printk("device_create error!\n");
        ret = PTR_ERR(blockio->pdevice);
        goto device_create_error;
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
    printk("blockio_init success!\n");
    return 0;

    led_init_error:
        for(; i >= 0; i--)
        {
            free_irq(blockio->key[i].irq_num,blockio);
            gpio_free(blockio->key[i].key_gpio);
        }
    key_init_error:
        device_destroy(blockio->pclass,blockio->devid);
    device_create_error:
        class_destroy(blockio->pclass);
    class_create_error:
        cdev_del(&blockio->cdev);
    cdev_add_error:
        unregister_chrdev_region(blockio->devid,BLOCKIO_COUNT);
    chrdev_region_error:
        del_timer_sync(&blockio->timer.timer);
        kfree(blockio);
    return ret;
}
static void __exit blockio_exit(void)
{
    int i = KEY_COUNT - 1;
    gpio_set_value(blockio->led.led_gpio,1);
    gpio_free(blockio->led.led_gpio);
    for(; i >= 0; i--)
    {
        free_irq(blockio->key[i].irq_num,blockio);
        gpio_free(blockio->key[i].key_gpio);
    }
    device_destroy(blockio->pclass,blockio->devid);
    class_destroy(blockio->pclass);
    cdev_del(&blockio->cdev);
    unregister_chrdev_region(blockio->devid,BLOCKIO_COUNT);
    del_timer_sync(&blockio->timer.timer);
    kfree(blockio);
    printk("blockio_exit success!\n");
}

module_init(blockio_init);
module_exit(blockio_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("dong");