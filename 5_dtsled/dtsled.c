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

#define  DTSLED_COUNT   1
#define  DTSLED_NAME    "dtsled"

#define  NODE_NAME      "/miniled"
#define  PROP_NAME1     "compatible"
#define  PROP_NAME2     "status"
#define  PROP_NAME3     "reg"

struct dtsled_dev{
    dev_t devid;
    int major;
    int minor;
    struct cdev cdev;
    struct class *pclass;
    struct device *pdevice;
};
struct dtsled_of{
    struct device_node *dn;
    struct property *prop;
    const char *strval[1];
    int elem_size;
    u32 *arrval;
};
struct dtsled_reg{
    void __iomem *CCM_CCGR1_VBASE;
    void __iomem *SW_MUX_GPIO1_IO03_VBASE;
    void __iomem *SW_PAD_GPIO1_IO03_VBASE;
    void __iomem *GPIO1_DR_VBASE;
    void __iomem *GPIO1_GDIR_VBASE;
};
struct dtsled_dev *dtsleddev;
struct dtsled_of *dtsledof;
struct dtsled_reg *dtsledreg;

int dtsled_open(struct inode *pinode, struct file *filp)
{
    //filp->private_data = dtsledreg->GPIO1_DR_VBASE;
    filp->private_data = dtsledreg;
    return 0;
}
ssize_t dtsled_write(struct file *filp, const char __user *pbuf, size_t count, loff_t *ploff)
{
    char data = 0;
    int ret = 0;
    u32 val = 0;
    struct dtsled_reg *reg = (struct dtsled_reg *)filp->private_data;
    ret = copy_from_user(&data,pbuf,count);
    if(ret != 0)
    {
        printk("copy_from_user error!\n");
        return -EFAULT;
    }
    printk("kernel write count:%d!\n",count);

    //val = readl((void __iomem *)filp->private_data);
    val = readl(reg->GPIO1_DR_VBASE);
    if(data == 1)
        val &=~ 8;
    else if(data == 0)
        val |= 8;
    else
    {
        printk("command error!\n");
        return -EFAULT;
    }
    //writel(val,(void __iomem *)filp->private_data);
    writel(val,reg->GPIO1_DR_VBASE);
    printk("newchrled_write success!\n");
    return 0;
}
int dtsled_close(struct inode *pinode, struct file *filp)
{
    return 0;
}

const struct file_operations dtsled_fops = {
    .owner = THIS_MODULE,
    .open = dtsled_open,
    .write = dtsled_write,
    .release = dtsled_close
};

static int __init dtsled_init(void)
{
    u32 val = 0;
    int ret = 0;
    dtsleddev = (struct dtsled_dev *)kmalloc(sizeof(struct dtsled_dev),GFP_KERNEL);
    if(!dtsleddev)
    {
        printk("dtsled_dev kmalloc error!\n");
        return -EINVAL;
    }
    dtsledof = (struct dtsled_of *)kmalloc(sizeof(struct dtsled_of),GFP_KERNEL);
    if(!dtsledof)
    {
        printk("dtsled_of kmalloc error!\n");
        ret = -EINVAL;
        goto dtsled_of_kmalloc_error;
    }
    dtsledreg = (struct dtsled_reg *)kmalloc(sizeof(struct dtsled_reg),GFP_KERNEL);
    if(!dtsledreg)
    {
        printk("dtsled_reg kmalloc error!\n");
        ret = -EINVAL;
        goto dtsled_reg_kmalloc_error;
    }

    dtsleddev->major = 0;
    dtsleddev->minor = 0;
    if(dtsleddev->major)
    {
        dtsleddev->devid = MKDEV(dtsleddev->major,dtsleddev->minor);
        ret = register_chrdev_region(dtsleddev->devid,DTSLED_COUNT,DTSLED_NAME);
        if(ret < 0)
        {
            printk("register_chrdev_region error!\n");
            goto chrdev_region_error;
        }
    }else
    {
        ret = alloc_chrdev_region(&dtsleddev->devid,dtsleddev->minor,DTSLED_COUNT,DTSLED_NAME);
        if(ret < 0)
        {
            printk("alloc_chrdev_region error!\n");
            goto chrdev_region_error;
        }
        dtsleddev->major = MAJOR(dtsleddev->devid);
        dtsleddev->minor = MINOR(dtsleddev->devid);
    }
    printk("dtsled major=%d,minor=%d!\n",dtsleddev->major,dtsleddev->minor);
    dtsleddev->cdev.owner = THIS_MODULE;
    cdev_init(&dtsleddev->cdev,&dtsled_fops);
    ret = cdev_add(&dtsleddev->cdev,dtsleddev->devid,DTSLED_COUNT);
    if(ret < 0)
    {
        printk("cdev_add error!\n");
        goto cdev_add_error;
    }
    dtsleddev->pclass = class_create(THIS_MODULE,DTSLED_NAME);
    if(IS_ERR(dtsleddev->pclass))
    {
        printk("class_create error!\n");
        ret = PTR_ERR(dtsleddev->pclass);
        goto class_create_error;
    }
    dtsleddev->pdevice = device_create(dtsleddev->pclass,NULL,dtsleddev->devid,NULL,DTSLED_NAME);
    if(IS_ERR(dtsleddev->pdevice))
    {
        printk("device_create error!\n");
        ret = PTR_ERR(dtsleddev->pdevice);
        goto device_create_error;
    }

    dtsledof->dn = of_find_node_by_path(NODE_NAME);
    if(!dtsledof->dn)
    {
        printk("of_find_node_by_path error!\n");
        ret = -EINVAL;
        goto find_node_error;
    }
    printk("node:%s!\n",dtsledof->dn->name);
    dtsledof->prop = of_find_property(dtsledof->dn,PROP_NAME1,NULL);
    if(!dtsledof->prop)
    {
        printk("of_find_property error!\n");
        ret = -EINVAL;
        goto find_prop1_error;
    }
    printk("%s:%s!\n",dtsledof->prop->name,(char *)dtsledof->prop->value);
    ret = of_property_read_string(dtsledof->dn,PROP_NAME2,dtsledof->strval);
    if(ret != 0)
    {
        printk("of_property_read_string error!\n");
        goto find_prop2_error;
    }
    printk("status:%s!\n",dtsledof->strval[0]);
#if 0
    dtsledof->elem_size = of_property_count_elems_of_size(dtsledof->dn,PROP_NAME3,sizeof(u32));
    if(dtsledof->elem_size < 0)
    {
        printk("of_property_count_elems_of_size error!\n");
        ret = dtsledof->elem_size;
        goto count_elems_error;
    }
    dtsledof->arrval = (u32 *)kmalloc(sizeof(u32) * dtsledof->elem_size,GFP_KERNEL);
    if(!dtsledof->arrval)
    {
        printk("arrval kmalloc error!\n");
        ret = -EINVAL;
        goto arrval_kmalloc_error;
    }
    ret = of_property_read_u32_array(dtsledof->dn,PROP_NAME3,dtsledof->arrval,dtsledof->elem_size);
    if(ret != 0)
    {
        printk("of_property_read_u32_array error!\n");
        goto find_prop3_error;
    }

    dtsledreg->CCM_CCGR1_VBASE = ioremap(dtsledof->arrval[0],dtsledof->arrval[1]);
    dtsledreg->SW_MUX_GPIO1_IO03_VBASE = ioremap(dtsledof->arrval[2],dtsledof->arrval[3]);
    dtsledreg->SW_PAD_GPIO1_IO03_VBASE = ioremap(dtsledof->arrval[4],dtsledof->arrval[5]);
    dtsledreg->GPIO1_DR_VBASE = ioremap(dtsledof->arrval[6],dtsledof->arrval[7]);
    dtsledreg->GPIO1_GDIR_VBASE = ioremap(dtsledof->arrval[8],dtsledof->arrval[9]);
#endif
    dtsledreg->CCM_CCGR1_VBASE = of_iomap(dtsledof->dn,0);
    dtsledreg->SW_MUX_GPIO1_IO03_VBASE = of_iomap(dtsledof->dn,1);
    dtsledreg->SW_PAD_GPIO1_IO03_VBASE = of_iomap(dtsledof->dn,2);
    dtsledreg->GPIO1_DR_VBASE = of_iomap(dtsledof->dn,3);
    dtsledreg->GPIO1_GDIR_VBASE = of_iomap(dtsledof->dn,4);
    val = readl(dtsledreg->CCM_CCGR1_VBASE);
    val |= (3<<26);
    writel(val,dtsledreg->CCM_CCGR1_VBASE);
    writel(5,dtsledreg->SW_MUX_GPIO1_IO03_VBASE);
    writel(0x10b0,dtsledreg->SW_PAD_GPIO1_IO03_VBASE);
    val = readl(dtsledreg->GPIO1_DR_VBASE);
    val &=~ 8;
    writel(val,dtsledreg->GPIO1_DR_VBASE);
    val = readl(dtsledreg->GPIO1_GDIR_VBASE);
    val |= 8;
    writel(val,dtsledreg->GPIO1_GDIR_VBASE);
    printk("dtsled_init success!\n");
    return 0;

#if 0
    find_prop3_error:
        kfree(dtsledof->arrval);
    arrval_kmalloc_error:
    count_elems_error:
#endif
    find_prop2_error:
    find_prop1_error:
    find_node_error:
        device_destroy(dtsleddev->pclass,dtsleddev->devid);
    device_create_error:
        class_destroy(dtsleddev->pclass);
    class_create_error:
        cdev_del(&dtsleddev->cdev);
    cdev_add_error:
        unregister_chrdev_region(dtsleddev->devid,DTSLED_COUNT);
    chrdev_region_error:
        kfree(dtsledreg);
    dtsled_reg_kmalloc_error:
        kfree(dtsledof);
    dtsled_of_kmalloc_error:
        kfree(dtsleddev);
    return ret;
}
static void __exit dtsled_exit(void)
{
    u32 val = 0;
    val = readl(dtsledreg->GPIO1_DR_VBASE);
    val |= 8;
    writel(val,dtsledreg->GPIO1_DR_VBASE);
    iounmap(dtsledreg->CCM_CCGR1_VBASE);
    iounmap(dtsledreg->SW_MUX_GPIO1_IO03_VBASE);
    iounmap(dtsledreg->SW_PAD_GPIO1_IO03_VBASE);
    iounmap(dtsledreg->GPIO1_DR_VBASE);
    iounmap(dtsledreg->GPIO1_GDIR_VBASE);

    device_destroy(dtsleddev->pclass,dtsleddev->devid);
    class_destroy(dtsleddev->pclass);
    cdev_del(&dtsleddev->cdev);
    unregister_chrdev_region(dtsleddev->devid,DTSLED_COUNT);

#if 0
    kfree(dtsledof->arrval);
#endif
    kfree(dtsledreg);
    kfree(dtsledof);
    kfree(dtsleddev);
    printk("dtsled_exit success!\n");
}

module_init(dtsled_init);
module_exit(dtsled_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("dong");