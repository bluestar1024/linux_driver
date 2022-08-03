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

#define PLATLED_COUNT     1
#define PLATLED_NAME      "platled"

typedef struct platled_prvdata_struct{
    int ccm_ccgr1_data;
    int ccm_ccgr1_shift;
    int sw_mux_gpio1_io03_data;
    int sw_pad_gpio1_io03_data;
    int gpio1_dr_pos;
    int gpio1_gdir_pos;
}plpd;
struct platled_dev{
    dev_t devid;
    int major;
    int minor;
    struct cdev cdev;
    struct class *pclass;
    struct device *pdevice;
    plpd *platled_prvdata;
};
struct platled_dev *platled;

void __iomem *CCM_CCGR1_VBASE;
void __iomem *SW_MUX_GPIO1_IO03_VBASE;
void __iomem *SW_PAD_GPIO1_IO03_VBASE;
void __iomem *GPIO1_DR_VBASE;
void __iomem *GPIO1_GDIR_VBASE;

int platled_open(struct inode *pinode, struct file *filp)
{
    return 0;
}
ssize_t platled_write(struct file *filp, const char __user *pbuf, size_t count, loff_t *ploff)
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
int platled_close(struct inode *pinode, struct file *filp)
{
    return 0;
}

const struct file_operations platled_fops = {
    .owner = THIS_MODULE,
    .open = platled_open,
    .write = platled_write,
    .release = platled_close
};

int platleddrv_probe(struct platform_device *pdev)
{
    //int i = 0;
    u32 val = 0;
    int ret = 0;
    struct resource *platleddrv_resource[pdev->num_resources];

    platled = (struct platled_dev *)kmalloc(sizeof(struct platled_dev),GFP_KERNEL);
    if(!platled)
    {
        printk("kmalloc error!\n");
        return -EINVAL;
    }

    platled->platled_prvdata = (plpd *)pdev->dev.platform_data;

    platleddrv_resource[0] = platform_get_resource_byname(pdev,IORESOURCE_MEM,"resource_mem0");
    platleddrv_resource[1] = platform_get_resource(pdev,IORESOURCE_MEM,1);
    platleddrv_resource[2] = platform_get_resource_byname(pdev,IORESOURCE_MEM,"resource_mem2");
    platleddrv_resource[3] = platform_get_resource(pdev,IORESOURCE_MEM,3);
    platleddrv_resource[4] = platform_get_resource_byname(pdev,IORESOURCE_MEM,"resource_mem4");
#if 0
    for(; i < pdev->num_resources; i++)
    {
        platleddrv_resource[i] = platform_get_resource(pdev,IORESOURCE_MEM,i);
    }
#endif
    CCM_CCGR1_VBASE = ioremap(platleddrv_resource[0]->start,resource_size(platleddrv_resource[0]));
    SW_MUX_GPIO1_IO03_VBASE = ioremap(platleddrv_resource[1]->start,resource_size(platleddrv_resource[1]));
    SW_PAD_GPIO1_IO03_VBASE = ioremap(platleddrv_resource[2]->start,resource_size(platleddrv_resource[2]));
    GPIO1_DR_VBASE = ioremap(platleddrv_resource[3]->start,resource_size(platleddrv_resource[3]));
    GPIO1_GDIR_VBASE = ioremap(platleddrv_resource[4]->start,resource_size(platleddrv_resource[4]));
#if 0
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
#endif
    val = readl(CCM_CCGR1_VBASE);
    val |= (platled->platled_prvdata->ccm_ccgr1_data << platled->platled_prvdata->ccm_ccgr1_shift);
    writel(val,CCM_CCGR1_VBASE);
    writel(platled->platled_prvdata->sw_mux_gpio1_io03_data,SW_MUX_GPIO1_IO03_VBASE);
    writel(platled->platled_prvdata->sw_pad_gpio1_io03_data,SW_PAD_GPIO1_IO03_VBASE);
    val = readl(GPIO1_DR_VBASE);
    val &=~ platled->platled_prvdata->gpio1_dr_pos;
    writel(val,GPIO1_DR_VBASE);
    val = readl(GPIO1_GDIR_VBASE);
    val |= platled->platled_prvdata->gpio1_gdir_pos;
    writel(val,GPIO1_GDIR_VBASE);

    platled->major = 0;
    platled->minor = 0;
    if(platled->major)
    {
        platled->devid = MKDEV(platled->major,platled->minor);
        ret = register_chrdev_region(platled->devid,PLATLED_COUNT,PLATLED_NAME);
        if(ret < 0)
        {
            printk("register_chrdev_region error!\n");
            goto chrdev_region_error;
        }
    }else
    {
        ret = alloc_chrdev_region(&platled->devid,0,PLATLED_COUNT,PLATLED_NAME);
        if(ret < 0)
        {
            printk("alloc_chrdev_region error!\n");
            goto chrdev_region_error;
        }
        platled->major = MAJOR(platled->devid);
        platled->minor = MINOR(platled->devid);
    }
    printk("platled major=%d,minor=%d!\n",platled->major,platled->minor);

    platled->cdev.owner = THIS_MODULE;
    cdev_init(&platled->cdev,&platled_fops);
    ret = cdev_add(&platled->cdev,platled->devid,PLATLED_COUNT);
    if(ret < 0)
    {
        printk("cdev_add error!\n");
        goto cdev_add_error;
    }

    platled->pclass = class_create(THIS_MODULE,PLATLED_NAME);
    if(IS_ERR(platled->pclass))
    {
        printk("class_create error!\n");
        ret = PTR_ERR(platled->pclass);
        goto class_create_error;
    }
    platled->pdevice = device_create(platled->pclass,NULL,platled->devid,NULL,PLATLED_NAME);
    if(IS_ERR(platled->pdevice))
    {
        printk("device_create error!\n");
        ret = PTR_ERR(platled->pdevice);
        goto device_create_error;
    }

    printk("probe success!\n");
    return 0;

    device_create_error:
        class_destroy(platled->pclass);
    class_create_error:
        cdev_del(&platled->cdev);
    cdev_add_error:
        unregister_chrdev_region(platled->devid,PLATLED_COUNT);
    chrdev_region_error:
        kfree(platled);
    return ret;
}
int platleddrv_remove(struct platform_device *pdev)
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

    device_destroy(platled->pclass,platled->devid);
    class_destroy(platled->pclass);
    cdev_del(&platled->cdev);
    unregister_chrdev_region(platled->devid,PLATLED_COUNT);
    printk("remove success!\n");
    return 0;
}
const struct platform_device_id platleddrv_id_table[] = {
    {"platled",0x1122},
    {"platled_xx",0x2233},
    {"platled_yy",0x3344},
};

struct platform_driver platleddrv = {
    .probe = platleddrv_probe,
    .remove = platleddrv_remove,
    .driver = {
        .name = "platled_AA"
    },
    .id_table = platleddrv_id_table
};

static int __init platleddrv_init(void)
{
    return platform_driver_register(&platleddrv);
}
static void __exit platleddrv_exit(void)
{
    platform_driver_unregister(&platleddrv);
}

module_init(platleddrv_init);
module_exit(platleddrv_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("dong");