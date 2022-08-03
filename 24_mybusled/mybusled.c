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

#define MYBUSLED_COUNT              1
#define MYBUSLED_NAME               "mybusled"

#define CCM_CCGR1_PBASE             0x020c406c
#define SW_MUX_GPIO1_IO03_PBASE     0x020e0068
#define SW_PAD_GPIO1_IO03_PBASE     0x020e02f4
#define GPIO1_DR_PBASE              0x0209c000
#define GPIO1_GDIR_PBASE            0x0209c004

#define LED_REG_LENGTH              4

struct mybus_driver{
    int drv_version;
    struct device_driver drv;
    int (*probe)(struct mybus_device *mbdev);
    int (*remove)(struct mybus_device *mbdev);
};
struct mybus_device{
    int dev_version;
    struct device dev;
    u32 num_resources;
    struct resource *resource;
};

typedef struct mybusled_prvdata_struct{
    int ccm_ccgr1_data;
    int ccm_ccgr1_shift;
    int sw_mux_gpio1_io03_data;
    int sw_pad_gpio1_io03_data;
    int gpio1_dr_pos;
    int gpio1_gdir_pos;
}mblpd;
struct mybusled_dev{
    dev_t devid;
    int major;
    int minor;
    struct cdev cdev;
    struct class *pclass;
    struct device *pdevice;
    mblpd *mybusled_prvdata;
};
struct mybusled_dev *mybusled;

void __iomem *CCM_CCGR1_VBASE;
void __iomem *SW_MUX_GPIO1_IO03_VBASE;
void __iomem *SW_PAD_GPIO1_IO03_VBASE;
void __iomem *GPIO1_DR_VBASE;
void __iomem *GPIO1_GDIR_VBASE;

mblpd mybusled_prvdata = {
    .ccm_ccgr1_data = 3,
    .ccm_ccgr1_shift = 26,
    .sw_mux_gpio1_io03_data = 5,
    .sw_pad_gpio1_io03_data = 0x10b0,
    .gpio1_dr_pos = 8,
    .gpio1_gdir_pos = 8
};

struct resource mybusdev_resource[] = {
    [0] = {
        .start = CCM_CCGR1_PBASE,
        .end = CCM_CCGR1_PBASE + LED_REG_LENGTH - 1,
        .name = "resource_mem0",
        .flags = IORESOURCE_MEM,
    },
    [1] = {
        .start = SW_MUX_GPIO1_IO03_PBASE,
        .end = SW_MUX_GPIO1_IO03_PBASE + LED_REG_LENGTH - 1,
        .name = "resource_mem1",
        .flags = IORESOURCE_MEM,
    },
    [2] = {
        .start = SW_PAD_GPIO1_IO03_PBASE,
        .end = SW_PAD_GPIO1_IO03_PBASE + LED_REG_LENGTH - 1,
        .name = "resource_mem2",
        .flags = IORESOURCE_MEM,
    },
    [3] = {
        .start = GPIO1_DR_PBASE,
        .end = GPIO1_DR_PBASE + LED_REG_LENGTH - 1,
        .name = "resource_mem3",
        .flags = IORESOURCE_MEM,
    },
    [4] = {
        .start = GPIO1_GDIR_PBASE,
        .end = GPIO1_GDIR_PBASE + LED_REG_LENGTH - 1,
        .name = "resource_mem4",
        .flags = IORESOURCE_MEM,
    }
};

int mybusled_open(struct inode *pinode, struct file *filp)
{
    return 0;
}
ssize_t mybusled_write(struct file *filp, const char __user *pbuf, size_t count, loff_t *ploff)
{
    char data = 0;
    int ret = 0;
    u32 val = 0;
    ret = copy_from_user(&data,pbuf,count);
    if(ret != 0)
    {
        printk("copy_from_user error!\n");
        return -EFAULT;
    }
    printk("kernel write count:%d!\n",count);

    val = readl(GPIO1_DR_VBASE);
    if(data == 1)
        val &=~ 8;
    else if(data == 0)
        val |= 8;
    else
    {
        printk("command error!\n");
        return -EFAULT;
    }
    writel(val,GPIO1_DR_VBASE);
    return 0;
}
int mybusled_close(struct inode *pinode, struct file *filp)
{
    return 0;
}

const struct file_operations mybusled_fops = {
    .owner = THIS_MODULE,
    .open = mybusled_open,
    .write = mybusled_write,
    .release = mybusled_close
};

int mybus_match(struct device *dev, struct device_driver *drv)
{
    struct mybus_driver *mbdrv = container_of(drv,struct mybus_driver,drv);
    struct mybus_device *mbdev = container_of(dev,struct mybus_device,dev);
    if(mbdrv->drv_version == mbdev->dev_version)
        return 1;
    return 0;
}
struct bus_type mybus = {
    .name = "mybus",
    .match = mybus_match
};

struct resource *mybus_get_resource(struct mybus_device *dev,unsigned int type, unsigned int num)
{
	int i = 0;
	for (; i < dev->num_resources; i++)
    {
		struct resource *r = &dev->resource[i];
		if(type == r->flags && num-- == 0)
			return r;
	}
	return NULL;
}
struct resource *mybus_get_resource_byname(struct mybus_device *dev,unsigned int type,const char *name)
{
	int i = 0;
	for (; i < dev->num_resources; i++)
    {
		struct resource *r = &dev->resource[i];
		if(type == r->flags && !strcmp(r->name, name))
			return r;
	}
	return NULL;
}

void dev_release(struct device *dev)
{
    printk("dev_release success!\n");
}
int mybusdrv_probe(struct mybus_device *mbdev)
{
    //int i = 0;
    u32 val = 0;
    int ret = 0;
    struct resource *mybusdrv_resource[mbdev->num_resources];

    mybusled = (struct mybusled_dev *)kmalloc(sizeof(struct mybusled_dev),GFP_KERNEL);
    if(!mybusled)
    {
        printk("kmalloc error!\n");
        return -EINVAL;
    }

    mybusled->mybusled_prvdata = (mblpd *)mbdev->dev.platform_data;

    mybusdrv_resource[0] = mybus_get_resource_byname(mbdev,IORESOURCE_MEM,"resource_mem0");
    mybusdrv_resource[1] = mybus_get_resource(mbdev,IORESOURCE_MEM,1);
    mybusdrv_resource[2] = mybus_get_resource_byname(mbdev,IORESOURCE_MEM,"resource_mem2");
    mybusdrv_resource[3] = mybus_get_resource(mbdev,IORESOURCE_MEM,3);
    mybusdrv_resource[4] = mybus_get_resource_byname(mbdev,IORESOURCE_MEM,"resource_mem4");

    CCM_CCGR1_VBASE = ioremap(mybusdrv_resource[0]->start,resource_size(mybusdrv_resource[0]));
    SW_MUX_GPIO1_IO03_VBASE = ioremap(mybusdrv_resource[1]->start,resource_size(mybusdrv_resource[1]));
    SW_PAD_GPIO1_IO03_VBASE = ioremap(mybusdrv_resource[2]->start,resource_size(mybusdrv_resource[2]));
    GPIO1_DR_VBASE = ioremap(mybusdrv_resource[3]->start,resource_size(mybusdrv_resource[3]));
    GPIO1_GDIR_VBASE = ioremap(mybusdrv_resource[4]->start,resource_size(mybusdrv_resource[4]));

    val = readl(CCM_CCGR1_VBASE);
    val |= (mybusled->mybusled_prvdata->ccm_ccgr1_data << mybusled->mybusled_prvdata->ccm_ccgr1_shift);
    writel(val,CCM_CCGR1_VBASE);
    writel(mybusled->mybusled_prvdata->sw_mux_gpio1_io03_data,SW_MUX_GPIO1_IO03_VBASE);
    writel(mybusled->mybusled_prvdata->sw_pad_gpio1_io03_data,SW_PAD_GPIO1_IO03_VBASE);
    val = readl(GPIO1_DR_VBASE);
    val &=~ mybusled->mybusled_prvdata->gpio1_dr_pos;
    writel(val,GPIO1_DR_VBASE);
    val = readl(GPIO1_GDIR_VBASE);
    val |= mybusled->mybusled_prvdata->gpio1_gdir_pos;
    writel(val,GPIO1_GDIR_VBASE);

    mybusled->major = 0;
    mybusled->minor = 0;
    if(mybusled->major)
    {
        mybusled->devid = MKDEV(mybusled->major,mybusled->minor);
        ret = register_chrdev_region(mybusled->devid,MYBUSLED_COUNT,MYBUSLED_NAME);
        if(ret < 0)
        {
            printk("register_chrdev_region error!\n");
            goto chrdev_region_error;
        }
    }else
    {
        ret = alloc_chrdev_region(&mybusled->devid,0,MYBUSLED_COUNT,MYBUSLED_NAME);
        if(ret < 0)
        {
            printk("alloc_chrdev_region error!\n");
            goto chrdev_region_error;
        }
        mybusled->major = MAJOR(mybusled->devid);
        mybusled->minor = MINOR(mybusled->devid);
    }
    printk("mybusled major=%d,minor=%d!\n",mybusled->major,mybusled->minor);

    mybusled->cdev.owner = THIS_MODULE;
    cdev_init(&mybusled->cdev,&mybusled_fops);
    ret = cdev_add(&mybusled->cdev,mybusled->devid,MYBUSLED_COUNT);
    if(ret < 0)
    {
        printk("cdev_add error!\n");
        goto cdev_add_error;
    }

    mybusled->pclass = class_create(THIS_MODULE,MYBUSLED_NAME);
    if(IS_ERR(mybusled->pclass))
    {
        printk("class_create error!\n");
        ret = PTR_ERR(mybusled->pclass);
        goto class_create_error;
    }
    mybusled->pdevice = device_create(mybusled->pclass,NULL,mybusled->devid,NULL,MYBUSLED_NAME);
    if(IS_ERR(mybusled->pdevice))
    {
        printk("device_create error!\n");
        ret = PTR_ERR(mybusled->pdevice);
        goto device_create_error;
    }

    printk("mybusdrv_probe success!\n");
    return 0;

    device_create_error:
        class_destroy(mybusled->pclass);
    class_create_error:
        cdev_del(&mybusled->cdev);
    cdev_add_error:
        unregister_chrdev_region(mybusled->devid,MYBUSLED_COUNT);
    chrdev_region_error:
        kfree(mybusled);
    return ret;
}
int drv_probe(struct device *dev)
{
    struct mybus_driver *mbdrv;
    struct mybus_device *mbdev;
    struct device_driver *drv = dev->driver;
    mbdrv = container_of(drv,struct mybus_driver,drv);
    mbdev = container_of(dev,struct mybus_device,dev);
    mbdrv->probe(mbdev);
    return 0;
}
int mybusdrv_remove(struct mybus_device *mbdev)
{
    u32 val = 0;
    val = readl(GPIO1_DR_VBASE);
    val |= 8;
    writel(val,GPIO1_DR_VBASE);
    iounmap(CCM_CCGR1_VBASE);
    iounmap(SW_MUX_GPIO1_IO03_VBASE);
    iounmap(SW_PAD_GPIO1_IO03_VBASE);
    iounmap(GPIO1_DR_VBASE);
    iounmap(GPIO1_GDIR_VBASE);

    device_destroy(mybusled->pclass,mybusled->devid);
    class_destroy(mybusled->pclass);
    cdev_del(&mybusled->cdev);
    unregister_chrdev_region(mybusled->devid,MYBUSLED_COUNT);
    printk("mybusdrv_remove success!\n");
    return 0;
}
int drv_remove(struct device *dev)
{
    struct mybus_driver *mbdrv;
    struct mybus_device *mbdev;
    struct device_driver *drv = dev->driver;
    mbdrv = container_of(drv,struct mybus_driver,drv);
    mbdev = container_of(dev,struct mybus_device,dev);
    mbdrv->remove(mbdev);
    return 0;
}

struct mybus_driver mybusdrv = {
    .drv_version = 0x1122,
    .probe = mybusdrv_probe,
    .remove = mybusdrv_remove
};
struct mybus_device mybusdev = {
    .dev_version = 0x1122,
    .dev = {
        .init_name = "mybusdev",
        .bus = &mybus,
        .platform_data = &mybusled_prvdata,
        .release = dev_release
    },
    .num_resources = ARRAY_SIZE(mybusdev_resource),
    .resource = mybusdev_resource
};

int mybus_driver_register(struct mybus_driver *drv)
{
    drv->drv.name = "mybusdrv";
    drv->drv.bus = &mybus;
    drv->drv.probe = drv_probe;
    drv->drv.remove = drv_remove;
    return driver_register(&drv->drv);
}
void mybus_driver_unregister(struct mybus_driver *drv)
{
	driver_unregister(&drv->drv);
}

static int __init mybusled_init(void)
{
    int ret = 0;
    ret = bus_register(&mybus);
    ret = mybus_driver_register(&mybusdrv);
    ret = device_register(&mybusdev.dev);
    return 0;
}
static void __exit mybusled_exit(void)
{
    mybus_driver_unregister(&mybusdrv);
    device_unregister(&mybusdev.dev);
    bus_unregister(&mybus);
}

module_init(mybusled_init);
module_exit(mybusled_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("dong");