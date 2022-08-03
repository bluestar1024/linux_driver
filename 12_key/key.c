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

#define  KEY_COUNT         1
#define  KEY_NAME          "key"

#define  NODE_NAME          "/key"
#define  PROP_NAME          "key-gpio"

struct key_dev{
    dev_t devid;
    int major;
    int minor;
    struct cdev cdev;
    struct class *pclass;
    struct device *pdevice;
    struct device_node *dn;
    int key_gpio;
    atomic_t keyvalue;
};
struct key_dev *key;

int key_open(struct inode *pinode, struct file *filp)
{
    filp->private_data = key;
    return 0;
}
ssize_t key_read(struct file *filp, char __user *pbuf, size_t count, loff_t *ploff)
{
    int value = 0;
    int ret = 0;
    struct key_dev *dev = (struct key_dev *)filp->private_data;
    //printk("kernel read count:%d!\n",count);
    if(gpio_get_value(dev->key_gpio) == 0)
    {
        while(!gpio_get_value(dev->key_gpio));
        atomic_inc(&dev->keyvalue);
    }
    else if(gpio_get_value(dev->key_gpio) < 0)
    {
        printk("get_gpio_value error!\n");
        return -EFAULT;
    }
    
    value = atomic_read(&dev->keyvalue);
    ret = copy_to_user(pbuf,&value,count);
    if(ret != 0)
    {
        printk("copy_to_user error!\n");
        return -EFAULT;
    }
    //printk("key_read success!\n");
    return 0;
}
int key_close(struct inode *pinode, struct file *filp)
{
    return 0;
}

const struct file_operations key_fops = {
    .owner = THIS_MODULE,
    .open = key_open,
    .read = key_read,
    .release = key_close
};

int keyio_init(struct key_dev *key)
{
    int ret = 0;
    key->dn = of_find_node_by_path(NODE_NAME);
    if(!key->dn)
    {
        printk("of_find_node_by_path error!\n");
        ret = -EINVAL;
        goto find_node_error;
    }
    key->key_gpio = of_get_named_gpio(key->dn,PROP_NAME,0);
    if(key->key_gpio < 0)
    {
        printk("of_get_named_gpio error!\n");
        ret = key->key_gpio;
        goto get_gpio_error;
    }
    printk("key gpio:%d!\n",key->key_gpio);
    ret = gpio_request(key->key_gpio,KEY_NAME);
    if(ret != 0)
    {
        printk("gpio_request error!\n");
        goto gpio_request_error;
    }
    ret = gpio_direction_input(key->key_gpio);
    if(ret != 0)
    {
        printk("gpio_direction_input error!\n");
        goto gpio_input_error;
    }
    return 0;

    gpio_input_error:
        gpio_free(key->key_gpio);
    gpio_request_error:
    get_gpio_error:
    find_node_error:
        device_destroy(key->pclass,key->devid);
    return ret;
}

static int __init key_init(void)
{
    int ret = 0;
    key = (struct key_dev *)kmalloc(sizeof(struct key_dev),GFP_KERNEL);
    if(!key)
    {
        printk("kmalloc error!\n");
        return -EINVAL;
    }
    atomic_set(&key->keyvalue,0);

    key->major = 0;
    key->minor = 0;
    if(key->major)
    {
        key->devid = MKDEV(key->major,key->minor);
        ret = register_chrdev_region(key->devid,KEY_COUNT,KEY_NAME);
        if(ret < 0)
        {
            printk("register_chrdev_region error!\n");
            goto chrdev_region_error;
        }
    }else
    {
        ret = alloc_chrdev_region(&key->devid,key->minor,KEY_COUNT,KEY_NAME);
        if(ret < 0)
        {
            printk("alloc_chrdev_region error!\n");
            goto chrdev_region_error;
        }
        key->major = MAJOR(key->devid);
        key->minor = MINOR(key->devid);
    }
    printk("key major:%d,minor:%d!\n",key->major,key->minor);
    key->cdev.owner = THIS_MODULE;
    cdev_init(&key->cdev,&key_fops);
    ret = cdev_add(&key->cdev,key->devid,KEY_COUNT);
    if(ret < 0)
    {
        printk("cdev_add error!\n");
        goto cdev_add_error;
    }
    key->pclass = class_create(THIS_MODULE,KEY_NAME);
    if(IS_ERR(key->pclass))
    {
        printk("class_create error!\n");
        ret = PTR_ERR(key->pclass);
        goto class_create_error;
    }
    key->pdevice = device_create(key->pclass,NULL,key->devid,NULL,KEY_NAME);
    if(IS_ERR(key->pdevice))
    {
        printk("device_create error!\n");
        ret = PTR_ERR(key->pdevice);
        goto device_create_error;
    }

    ret = keyio_init(key);
    if(ret != 0)
    {
        printk("keyio_init error!\n");
        goto device_create_error;
    }
    printk("key_init success!\n");
    return 0;

    device_create_error:
        class_destroy(key->pclass);
    class_create_error:
        cdev_del(&key->cdev);
    cdev_add_error:
        unregister_chrdev_region(key->devid,KEY_COUNT);
    chrdev_region_error:
        kfree(key);
    return ret;
}
static void __exit key_exit(void)
{
    gpio_free(key->key_gpio);
    device_destroy(key->pclass,key->devid);
    class_destroy(key->pclass);
    cdev_del(&key->cdev);
    unregister_chrdev_region(key->devid,KEY_COUNT);
    kfree(key);
    printk("key_exit success!\n");
}

module_init(key_init);
module_exit(key_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("dong");