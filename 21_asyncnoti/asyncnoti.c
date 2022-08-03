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

#define  ASYNCNOTI_COUNT         1
#define  ASYNCNOTI_NAME          "asyncnoti"

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
struct asyncnoti_dev{
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
    struct fasync_struct *fasync;
};
struct asyncnoti_dev *asyncnoti;

irqreturn_t irq_handle(int irq_num, void *dev_id)
{
    struct asyncnoti_dev *dev = (struct asyncnoti_dev *)dev_id;
    //tasklet_schedule(&dev->key[0].tasklet);
    schedule_work(&dev->key[0].work);
    return IRQ_HANDLED;
}
#if 0
void key_tasklet(unsigned long data)
{
    struct asyncnoti_dev *dev = (struct asyncnoti_dev *)data;
    dev->timer.timer.data = data;
    mod_timer(&dev->timer.timer,jiffies + msecs_to_jiffies(20));
}
#endif
void key_work(struct work_struct *work)
{
    struct irq_keydesc *key0_struct_ptr = container_of(work,struct irq_keydesc,work);
    struct asyncnoti_dev *dev = container_of(key0_struct_ptr,struct asyncnoti_dev,key[0]);
    dev->curkeynum = 0;
    dev->timer.timer.data = (unsigned long)dev;
    mod_timer(&dev->timer.timer,jiffies + msecs_to_jiffies(20));
}
void timer_handle(unsigned long arg)
{
    unsigned char num = 0;
    unsigned char value = 0;
    struct asyncnoti_dev *dev = (struct asyncnoti_dev *)arg;
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
        //wake_up(&dev->read_wait_head);
        kill_fasync(&dev->fasync,SIGIO,POLL_IN);
    }
}

int asyncnoti_open(struct inode *pinode, struct file *filp)
{
    filp->private_data = asyncnoti;
    return 0;
}
ssize_t asyncnoti_read(struct file *filp, char __user *pbuf, size_t count, loff_t *ploff)
{
    int keyvalue = 0;
    int releasekey = 0;
    int ret = 0;
    struct asyncnoti_dev *dev = (struct asyncnoti_dev *)filp->private_data;

    if(filp->f_flags & O_NONBLOCK){
        ;
    }else
    {
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
#if 0
unsigned int asyncnoti_poll(struct file *filp, struct poll_table_struct *wait)
{
    unsigned int mask = 0;
    //static int i = 1;
    struct asyncnoti_dev *dev = (struct asyncnoti_dev *)filp->private_data;

    poll_wait(filp,&dev->read_wait_head,wait);

    if(atomic_read(&dev->releasekey)){
        mask = POLLIN | POLLRDNORM;
    }
    
    //printk("i:%d!\n",i++);
    return mask;
}
#endif
int asyncnoti_fasync(int fd, struct file *filp, int on)
{
    struct asyncnoti_dev *dev = (struct asyncnoti_dev *)filp->private_data;
    return fasync_helper(fd,filp,on,&dev->fasync);
}
int asyncnoti_close(struct inode *pinode, struct file *filp)
{
    return asyncnoti_fasync(-1,filp,0);
}

const struct file_operations asyncnoti_fops = {
    .owner = THIS_MODULE,
    .open = asyncnoti_open,
    .read = asyncnoti_read,
    //.poll = asyncnoti_poll,
    .fasync = asyncnoti_fasync,
    .release = asyncnoti_close
};

int keyio_init(void)
{
    int i = 0, j = 0;
    int ret = 0;

    asyncnoti->dn = of_find_node_by_path(KEY_NODE);
    if(!asyncnoti->dn)
    {
        printk("key:of_find_node_by_path error!\n");
        return -EINVAL;
    }

    asyncnoti->key[0].irq_handler_name = irq_handle;
    //asyncnoti->key[0].key_tasklet_name = key_tasklet;
    asyncnoti->key[0].key_work_name = key_work;

    for(; i < KEY_COUNT; i++)
    {
        asyncnoti->key[i].key_gpio = of_get_named_gpio(asyncnoti->dn,KEY_PROP,i);
        if(asyncnoti->key[i].key_gpio < 0)
        {
            printk("key:of_get_named_gpio error!\n");
            ret = asyncnoti->key[i].key_gpio;
            goto get_gpio_error;
        }

        memset(asyncnoti->key[i].name,0,sizeof(asyncnoti->key[i].name));
        sprintf(asyncnoti->key[i].name,"GPIO_KEY%d",i);
        ret = gpio_request(asyncnoti->key[i].key_gpio,asyncnoti->key[i].name);
        if(ret)
        {
            printk("key:gpio_request error!\n");
            goto request_gpio_error;
        }

        ret = gpio_direction_input(asyncnoti->key[i].key_gpio);
        if(ret)
        {
            printk("key:gpio_direction_input error!\n");
            goto gpio_input_error;
        }

    #if 0
        asyncnoti->key[i].irq_num = gpio_to_irq(asyncnoti->key[i].key_gpio);
    #endif
    #if 1
        asyncnoti->key[i].irq_num = irq_of_parse_and_map(asyncnoti->dn,i);
    #endif

        memset(asyncnoti->key[i].name,0,sizeof(asyncnoti->key[i].name));
        sprintf(asyncnoti->key[i].name,"IRQ_KEY%d",i);
        ret = request_irq(asyncnoti->key[i].irq_num,asyncnoti->key[i].irq_handler_name,IRQF_TRIGGER_RISING|IRQF_TRIGGER_FALLING,asyncnoti->key[i].name,asyncnoti);
        if(ret)
        {
            printk("request_irq error!\n");
            goto request_irq_error;
        }

        //tasklet_init(&asyncnoti->key[i].tasklet,asyncnoti->key[i].key_tasklet_name,(unsigned long)asyncnoti);
        INIT_WORK(&asyncnoti->key[i].work,asyncnoti->key[i].key_work_name);
    }
    return 0;

    request_irq_error:
        j = i;
        for(j = j - 1; j >= 0; j--)
            free_irq(asyncnoti->key[j].irq_num,asyncnoti);
    gpio_input_error:
        gpio_free(asyncnoti->key[i].key_gpio);
    request_gpio_error:
        for(i = i - 1; i >= 0; i--)
            gpio_free(asyncnoti->key[i].key_gpio);
    get_gpio_error:
    return ret;
}
int led_init(void)
{
    int ret = 0;
    asyncnoti->dn = of_find_node_by_path(LED_NODE);
    if(!asyncnoti->dn)
    {
        printk("led:of_find_node_by_path error!\n");
        return -EINVAL;
    }

    asyncnoti->led.led_gpio = of_get_named_gpio(asyncnoti->dn,LED_PROP,0);
    if(asyncnoti->led.led_gpio < 0)
    {
        printk("led:of_get_named_gpio error!\n");
        ret = asyncnoti->led.led_gpio;
        goto get_gpio_error;
    }

    memset(asyncnoti->led.name,0,sizeof(asyncnoti->led.name));
    sprintf(asyncnoti->led.name,"GPIO_LED");
    ret = gpio_request(asyncnoti->led.led_gpio,asyncnoti->led.name);
    if(ret)
    {
        printk("led:gpio_request error!\n");
        goto request_gpio_error;
    }
    ret = gpio_direction_output(asyncnoti->led.led_gpio,1);
    if(ret)
    {
        printk("led:gpio_direction_output error!\n");
        goto gpio_output_error;
    }
    gpio_set_value(asyncnoti->led.led_gpio,0);
    return 0;

    gpio_output_error:
        gpio_free(asyncnoti->led.led_gpio);
    request_gpio_error:
    get_gpio_error:
    return ret;
}

static int __init asyncnoti_init(void)
{
    int i = KEY_COUNT - 1;
    int ret = 0;
    asyncnoti = (struct asyncnoti_dev *)kmalloc(sizeof(struct asyncnoti_dev),GFP_KERNEL);
    if(!asyncnoti)
    {
        printk("kmalloc error!\n");
        return -EINVAL;
    }
    init_timer(&asyncnoti->timer.timer);
    asyncnoti->timer.timer.function = timer_handle;
    atomic_set(&asyncnoti->keyvalue,INVALUE);
    atomic_set(&asyncnoti->releasekey,0);
    init_waitqueue_head(&asyncnoti->read_wait_head);

    asyncnoti->major = 0;
    asyncnoti->minor = 0;
    if(asyncnoti->major)
    {
        asyncnoti->devid = MKDEV(asyncnoti->major,asyncnoti->minor);
        ret = register_chrdev_region(asyncnoti->devid,ASYNCNOTI_COUNT,ASYNCNOTI_NAME);
        if(ret < 0)
        {
            printk("register_chrdev_region error!\n");
            goto chrdev_region_error;
        }
    }else
    {
        ret = alloc_chrdev_region(&asyncnoti->devid,asyncnoti->minor,ASYNCNOTI_COUNT,ASYNCNOTI_NAME);
        if(ret < 0)
        {
            printk("alloc_chrdev_region error!\n");
            goto chrdev_region_error;
        }
        asyncnoti->major = MAJOR(asyncnoti->devid);
        asyncnoti->minor = MINOR(asyncnoti->devid);
    }
    printk("asyncnoti major:%d,minor:%d!\n",asyncnoti->major,asyncnoti->minor);
    asyncnoti->cdev.owner = THIS_MODULE;
    cdev_init(&asyncnoti->cdev,&asyncnoti_fops);
    ret = cdev_add(&asyncnoti->cdev,asyncnoti->devid,ASYNCNOTI_COUNT);
    if(ret < 0)
    {
        printk("cdev_add error!\n");
        goto cdev_add_error;
    }
    asyncnoti->pclass = class_create(THIS_MODULE,ASYNCNOTI_NAME);
    if(IS_ERR(asyncnoti->pclass))
    {
        printk("class_create error!\n");
        ret = PTR_ERR(asyncnoti->pclass);
        goto class_create_error;
    }
    asyncnoti->pdevice = device_create(asyncnoti->pclass,NULL,asyncnoti->devid,NULL,ASYNCNOTI_NAME);
    if(IS_ERR(asyncnoti->pdevice))
    {
        printk("device_create error!\n");
        ret = PTR_ERR(asyncnoti->pdevice);
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
    printk("asyncnoti_init success!\n");
    return 0;

    led_init_error:
        for(; i >= 0; i--)
        {
            free_irq(asyncnoti->key[i].irq_num,asyncnoti);
            gpio_free(asyncnoti->key[i].key_gpio);
        }
    key_init_error:
        device_destroy(asyncnoti->pclass,asyncnoti->devid);
    device_create_error:
        class_destroy(asyncnoti->pclass);
    class_create_error:
        cdev_del(&asyncnoti->cdev);
    cdev_add_error:
        unregister_chrdev_region(asyncnoti->devid,ASYNCNOTI_COUNT);
    chrdev_region_error:
        del_timer_sync(&asyncnoti->timer.timer);
        kfree(asyncnoti);
    return ret;
}
static void __exit asyncnoti_exit(void)
{
    int i = KEY_COUNT - 1;
    gpio_set_value(asyncnoti->led.led_gpio,1);
    gpio_free(asyncnoti->led.led_gpio);
    for(; i >= 0; i--)
    {
        free_irq(asyncnoti->key[i].irq_num,asyncnoti);
        gpio_free(asyncnoti->key[i].key_gpio);
    }
    device_destroy(asyncnoti->pclass,asyncnoti->devid);
    class_destroy(asyncnoti->pclass);
    cdev_del(&asyncnoti->cdev);
    unregister_chrdev_region(asyncnoti->devid,ASYNCNOTI_COUNT);
    del_timer_sync(&asyncnoti->timer.timer);
    kfree(asyncnoti);
    printk("asyncnoti_exit success!\n");
}

module_init(asyncnoti_init);
module_exit(asyncnoti_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("dong");