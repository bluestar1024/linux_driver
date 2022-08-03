#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/io.h>

#define LED_MAJOR   200
#define LED_NAME    "led"

#define CCM_CCGR1_PBASE             0x020c406c
#define SW_MUX_GPIO1_IO03_PBASE     0x020e0068
#define SW_PAD_GPIO1_IO03_PBASE     0x020e02f4
#define GPIO1_DR_PBASE              0x0209c000
#define GPIO1_GDIR_PBASE            0x0209c004

void __iomem *CCM_CCGR1_VBASE;
void __iomem *SW_MUX_GPIO1_IO03_VBASE;
void __iomem *SW_PAD_GPIO1_IO03_VBASE;
void __iomem *GPIO1_DR_VBASE;
void __iomem *GPIO1_GDIR_VBASE;

int led_open(struct inode *pinode, struct file *filp)
{
    printk("led_open success!\n");
    return 0;
}
ssize_t led_write(struct file *filp, const char __user *pbuf, size_t count, loff_t *ploff)
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
        printk("command error!\n");
    writel(val,GPIO1_DR_VBASE);

    printk("led_write success!\n");
    return 0;
}
int led_close(struct inode *pinode, struct file *filp)
{
    printk("led_close success!\n");
    return 0;
}

const struct file_operations led_fops = {
    .owner = THIS_MODULE,
    .open = led_open,
    .write = led_write,
    .release = led_close
};

static int __init led_init(void)
{
    int ret = 0;
    u32 val = 0;

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

    ret = register_chrdev(LED_MAJOR,LED_NAME,&led_fops);
    if(ret < 0)
    {
        printk("register_chrdev error!\n");
        return -EIO;
    }
    printk("led_init success!\n");
    return 0;
}
static void __exit led_exit(void)
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

    unregister_chrdev(LED_MAJOR,LED_NAME);
    printk("led_exit success!\n");
}

module_init(led_init);
module_exit(led_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("dong");