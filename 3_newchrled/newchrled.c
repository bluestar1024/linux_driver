#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/io.h>
#include <linux/cdev.h>
#include <linux/device.h>

#define NEWCHRLED_COUNT     1
#define NEWCHRLED_NAME      "newchrled"

#define CCM_CCGR1_PBASE             0x020c406c
#define SW_MUX_GPIO1_IO03_PBASE     0x020e0068
#define SW_PAD_GPIO1_IO03_PBASE     0x020e02f4
#define GPIO1_DR_PBASE              0x0209c000
#define GPIO1_GDIR_PBASE            0x0209c004

struct newchrled_dev{
    dev_t devid;
    int major;
    int minor;
    struct cdev cdev;
    struct class *pclass;
    struct device *pdevice;
};
struct newchrled_dev newchrled;

void __iomem *CCM_CCGR1_VBASE;
void __iomem *SW_MUX_GPIO1_IO03_VBASE;
void __iomem *SW_PAD_GPIO1_IO03_VBASE;
void __iomem *GPIO1_DR_VBASE;
void __iomem *GPIO1_GDIR_VBASE;

int newchrled_open(struct inode *pinode, struct file *filp)
{
    printk("newchrled_open success!\n");
    return 0;
}
ssize_t newchrled_write(struct file *filp, const char __user *pbuf, size_t count, loff_t *ploff)
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
    printk("newchrled_write success!\n");
    return 0;
}
int newchrled_close(struct inode *pinode, struct file *filp)
{
    printk("newchrled_close success!\n");
    return 0;
}

const struct file_operations newchrled_fops = {
    .owner = THIS_MODULE,
    .open = newchrled_open,
    .write = newchrled_write,
    .release = newchrled_close
};

static int __init newchrled_init(void)
{
    u32 val = 0;
    int ret = 0;

    CCM_CCGR1_VBASE = ioremap(CCM_CCGR1_PBASE,4);
    SW_MUX_GPIO1_IO03_VBASE = ioremap(SW_MUX_GPIO1_IO03_PBASE,4);
    SW_PAD_GPIO1_IO03_VBASE = ioremap(SW_PAD_GPIO1_IO03_PBASE,4);
    GPIO1_DR_VBASE = ioremap(GPIO1_DR_PBASE,4);
    GPIO1_GDIR_VBASE = ioremap(GPIO1_GDIR_PBASE,4);
    val = readl(CCM_CCGR1_VBASE);
    val |= (3<<26);
    writel(val,CCM_CCGR1_VBASE);
    writel(5,SW_MUX_GPIO1_IO03_VBASE);
    writel(0x10b0,SW_PAD_GPIO1_IO03_VBASE);
    val = readl(GPIO1_DR_VBASE);
    val &=~ 8;
    writel(val,GPIO1_DR_VBASE);
    val = readl(GPIO1_GDIR_VBASE);
    val |= 8;
    writel(val,GPIO1_GDIR_VBASE);

    if(newchrled.major)
    {
        newchrled.devid = MKDEV(newchrled.major,0);
        ret = register_chrdev_region(newchrled.devid,NEWCHRLED_COUNT,NEWCHRLED_NAME);
        if(ret < 0)
        {
            printk("register_chrdev_region error!\n");
            goto chrdev_region_error;
        }
    }else
    {
        ret = alloc_chrdev_region(&newchrled.devid,0,NEWCHRLED_COUNT,NEWCHRLED_NAME);
        if(ret < 0)
        {
            printk("alloc_chrdev_region error!\n");
            goto chrdev_region_error;
        }
        newchrled.major = MAJOR(newchrled.devid);
        newchrled.minor = MINOR(newchrled.devid);
    }
    printk("newchrled major=%d,minor=%d!\n",newchrled.major,newchrled.minor);

    newchrled.cdev.owner = THIS_MODULE;
    cdev_init(&newchrled.cdev,&newchrled_fops);
    ret = cdev_add(&newchrled.cdev,newchrled.devid,NEWCHRLED_COUNT);
    if(ret < 0)
    {
        printk("cdev_add error!\n");
        goto cdev_add_error;
    }

    newchrled.pclass = class_create(THIS_MODULE,NEWCHRLED_NAME);
    if(IS_ERR(newchrled.pclass))
    {
        printk("class_create error!\n");
        ret = PTR_ERR(newchrled.pclass);
        goto class_create_error;
    }
    newchrled.pdevice = device_create(newchrled.pclass,NULL,newchrled.devid,NULL,NEWCHRLED_NAME);
    if(IS_ERR(newchrled.pdevice))
    {
        printk("device_create error!\n");
        ret = PTR_ERR(newchrled.pdevice);
        goto device_create_error;
    }

    printk("newchrled_init success!\n");
    return 0;

    device_create_error:
        class_destroy(newchrled.pclass);
    class_create_error:
        cdev_del(&newchrled.cdev);
    cdev_add_error:
        unregister_chrdev_region(newchrled.devid,NEWCHRLED_COUNT);
    chrdev_region_error:
    return ret;
}
static void __exit newchrled_exit(void)
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

    device_destroy(newchrled.pclass,newchrled.devid);
    class_destroy(newchrled.pclass);
    cdev_del(&newchrled.cdev);
    unregister_chrdev_region(newchrled.devid,NEWCHRLED_COUNT);
    printk("newchrled_exit success!\n");
}

module_init(newchrled_init);
module_exit(newchrled_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("dong");
