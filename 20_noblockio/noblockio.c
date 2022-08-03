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

#define  NOBLOCKIO_COUNT         1
#define  NOBLOCKIO_NAME          "noblockio"

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
struct noblockio_dev{
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
struct noblockio_dev *noblockio;

irqreturn_t irq_handle(int irq_num, void *dev_id)
{
    struct noblockio_dev *dev = (struct noblockio_dev *)dev_id;
    //tasklet_schedule(&dev->key[0].tasklet);
    schedule_work(&dev->key[0].work);
    return IRQ_HANDLED;
}
#if 0
void key_tasklet(unsigned long data)
{
    struct noblockio_dev *dev = (struct noblockio_dev *)data;
    dev->timer.timer.data = data;
    mod_timer(&dev->timer.timer,jiffies + msecs_to_jiffies(20));
}
#endif
void key_work(struct work_struct *work)
{
    struct irq_keydesc *key0_struct_ptr = container_of(work,struct irq_keydesc,work);
    struct noblockio_dev *dev = container_of(key0_struct_ptr,struct noblockio_dev,key[0]);
    dev->curkeynum = 0;
    dev->timer.timer.data = (unsigned long)dev;
    mod_timer(&dev->timer.timer,jiffies + msecs_to_jiffies(20));
}
void timer_handle(unsigned long arg)
{
    unsigned char num = 0;
    unsigned char value = 0;
    struct noblockio_dev *dev = (struct noblockio_dev *)arg;
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

int noblockio_open(struct inode *pinode, struct file *filp)
{
    filp->private_data = noblockio;
    return 0;
}
ssize_t noblockio_read(struct file *filp, char __user *pbuf, size_t count, loff_t *ploff)
{
    int keyvalue = 0;
    int releasekey = 0;
    int ret = 0;
    struct noblockio_dev *dev = (struct noblockio_dev *)filp->private_data;

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
unsigned int noblockio_poll(struct file *filp, struct poll_table_struct *wait)
{
    unsigned int mask = 0;
    //static int i = 1;
    struct noblockio_dev *dev = (struct noblockio_dev *)filp->private_data;

    poll_wait(filp,&dev->read_wait_head,wait);

    if(atomic_read(&dev->releasekey)){
        mask = POLLIN | POLLRDNORM;
    }
    
    //printk("i:%d!\n",i++);
    return mask;
}
int noblockio_close(struct inode *pinode, struct file *filp)
{
    return 0;
}

const struct file_operations noblockio_fops = {
    .owner = THIS_MODULE,
    .open = noblockio_open,
    .read = noblockio_read,
    .poll = noblockio_poll,
    .release = noblockio_close
};

int keyio_init(void)
{
    int i = 0, j = 0;
    int ret = 0;

    noblockio->dn = of_find_node_by_path(KEY_NODE);
    if(!noblockio->dn)
    {
        printk("key:of_find_node_by_path error!\n");
        return -EINVAL;
    }

    noblockio->key[0].irq_handler_name = irq_handle;
    //noblockio->key[0].key_tasklet_name = key_tasklet;
    noblockio->key[0].key_work_name = key_work;

    for(; i < KEY_COUNT; i++)
    {
        noblockio->key[i].key_gpio = of_get_named_gpio(noblockio->dn,KEY_PROP,i);
        if(noblockio->key[i].key_gpio < 0)
        {
            printk("key:of_get_named_gpio error!\n");
            ret = noblockio->key[i].key_gpio;
            goto get_gpio_error;
        }

        memset(noblockio->key[i].name,0,sizeof(noblockio->key[i].name));
        sprintf(noblockio->key[i].name,"GPIO_KEY%d",i);
        ret = gpio_request(noblockio->key[i].key_gpio,noblockio->key[i].name);
        if(ret)
        {
            printk("key:gpio_request error!\n");
            goto request_gpio_error;
        }

        ret = gpio_direction_input(noblockio->key[i].key_gpio);
        if(ret)
        {
            printk("key:gpio_direction_input error!\n");
            goto gpio_input_error;
        }

    #if 0
        noblockio->key[i].irq_num = gpio_to_irq(noblockio->key[i].key_gpio);
    #endif
    #if 1
        noblockio->key[i].irq_num = irq_of_parse_and_map(noblockio->dn,i);
    #endif

        memset(noblockio->key[i].name,0,sizeof(noblockio->key[i].name));
        sprintf(noblockio->key[i].name,"IRQ_KEY%d",i);
        ret = request_irq(noblockio->key[i].irq_num,noblockio->key[i].irq_handler_name,IRQF_TRIGGER_RISING|IRQF_TRIGGER_FALLING,noblockio->key[i].name,noblockio);
        if(ret)
        {
            printk("request_irq error!\n");
            goto request_irq_error;
        }

        //tasklet_init(&noblockio->key[i].tasklet,noblockio->key[i].key_tasklet_name,(unsigned long)noblockio);
        INIT_WORK(&noblockio->key[i].work,noblockio->key[i].key_work_name);
    }
    return 0;

    request_irq_error:
        j = i;
        for(j = j - 1; j >= 0; j--)
            free_irq(noblockio->key[j].irq_num,noblockio);
    gpio_input_error:
        gpio_free(noblockio->key[i].key_gpio);
    request_gpio_error:
        for(i = i - 1; i >= 0; i--)
            gpio_free(noblockio->key[i].key_gpio);
    get_gpio_error:
    return ret;
}
int led_init(void)
{
    int ret = 0;
    noblockio->dn = of_find_node_by_path(LED_NODE);
    if(!noblockio->dn)
    {
        printk("led:of_find_node_by_path error!\n");
        return -EINVAL;
    }

    noblockio->led.led_gpio = of_get_named_gpio(noblockio->dn,LED_PROP,0);
    if(noblockio->led.led_gpio < 0)
    {
        printk("led:of_get_named_gpio error!\n");
        ret = noblockio->led.led_gpio;
        goto get_gpio_error;
    }

    memset(noblockio->led.name,0,sizeof(noblockio->led.name));
    sprintf(noblockio->led.name,"GPIO_LED");
    ret = gpio_request(noblockio->led.led_gpio,noblockio->led.name);
    if(ret)
    {
        printk("led:gpio_request error!\n");
        goto request_gpio_error;
    }
    ret = gpio_direction_output(noblockio->led.led_gpio,1);
    if(ret)
    {
        printk("led:gpio_direction_output error!\n");
        goto gpio_output_error;
    }
    gpio_set_value(noblockio->led.led_gpio,0);
    return 0;

    gpio_output_error:
        gpio_free(noblockio->led.led_gpio);
    request_gpio_error:
    get_gpio_error:
    return ret;
}

static int __init noblockio_init(void)
{
    int i = KEY_COUNT - 1;
    int ret = 0;
    noblockio = (struct noblockio_dev *)kmalloc(sizeof(struct noblockio_dev),GFP_KERNEL);
    if(!noblockio)
    {
        printk("kmalloc error!\n");
        return -EINVAL;
    }
    init_timer(&noblockio->timer.timer);
    noblockio->timer.timer.function = timer_handle;
    atomic_set(&noblockio->keyvalue,INVALUE);
    atomic_set(&noblockio->releasekey,0);
    init_waitqueue_head(&noblockio->read_wait_head);

    noblockio->major = 0;
    noblockio->minor = 0;
    if(noblockio->major)
    {
        noblockio->devid = MKDEV(noblockio->major,noblockio->minor);
        ret = register_chrdev_region(noblockio->devid,NOBLOCKIO_COUNT,NOBLOCKIO_NAME);
        if(ret < 0)
        {
            printk("register_chrdev_region error!\n");
            goto chrdev_region_error;
        }
    }else
    {
        ret = alloc_chrdev_region(&noblockio->devid,noblockio->minor,NOBLOCKIO_COUNT,NOBLOCKIO_NAME);
        if(ret < 0)
        {
            printk("alloc_chrdev_region error!\n");
            goto chrdev_region_error;
        }
        noblockio->major = MAJOR(noblockio->devid);
        noblockio->minor = MINOR(noblockio->devid);
    }
    printk("noblockio major:%d,minor:%d!\n",noblockio->major,noblockio->minor);
    noblockio->cdev.owner = THIS_MODULE;
    cdev_init(&noblockio->cdev,&noblockio_fops);
    ret = cdev_add(&noblockio->cdev,noblockio->devid,NOBLOCKIO_COUNT);
    if(ret < 0)
    {
        printk("cdev_add error!\n");
        goto cdev_add_error;
    }
    noblockio->pclass = class_create(THIS_MODULE,NOBLOCKIO_NAME);
    if(IS_ERR(noblockio->pclass))
    {
        printk("class_create error!\n");
        ret = PTR_ERR(noblockio->pclass);
        goto class_create_error;
    }
    noblockio->pdevice = device_create(noblockio->pclass,NULL,noblockio->devid,NULL,NOBLOCKIO_NAME);
    if(IS_ERR(noblockio->pdevice))
    {
        printk("device_create error!\n");
        ret = PTR_ERR(noblockio->pdevice);
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
    printk("noblockio_init success!\n");
    return 0;

    led_init_error:
        for(; i >= 0; i--)
        {
            free_irq(noblockio->key[i].irq_num,noblockio);
            gpio_free(noblockio->key[i].key_gpio);
        }
    key_init_error:
        device_destroy(noblockio->pclass,noblockio->devid);
    device_create_error:
        class_destroy(noblockio->pclass);
    class_create_error:
        cdev_del(&noblockio->cdev);
    cdev_add_error:
        unregister_chrdev_region(noblockio->devid,NOBLOCKIO_COUNT);
    chrdev_region_error:
        del_timer_sync(&noblockio->timer.timer);
        kfree(noblockio);
    return ret;
}
static void __exit noblockio_exit(void)
{
    int i = KEY_COUNT - 1;
    gpio_set_value(noblockio->led.led_gpio,1);
    gpio_free(noblockio->led.led_gpio);
    for(; i >= 0; i--)
    {
        free_irq(noblockio->key[i].irq_num,noblockio);
        gpio_free(noblockio->key[i].key_gpio);
    }
    device_destroy(noblockio->pclass,noblockio->devid);
    class_destroy(noblockio->pclass);
    cdev_del(&noblockio->cdev);
    unregister_chrdev_region(noblockio->devid,NOBLOCKIO_COUNT);
    del_timer_sync(&noblockio->timer.timer);
    kfree(noblockio);
    printk("noblockio_exit success!\n");
}

module_init(noblockio_init);
module_exit(noblockio_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("dong");