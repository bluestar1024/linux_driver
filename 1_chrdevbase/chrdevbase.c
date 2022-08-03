#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/uaccess.h>

#define CHRDEVBASE_MAJOR    200
#define CHRDEVBASE_NAME     "chrdevbase"

char writebuf[50],readbuf[50];
char kerneldata[] = {"kernel data!"};

int chrdevbase_open(struct inode *pinode, struct file *filp)
{
    printk("chrdevbase_open success!\n");
    return 0;
}
ssize_t chrdevbase_write(struct file *filp, const char __user *pbuf, size_t count, loff_t *offt)
{
    int ret = 0;
    ret = copy_from_user(writebuf,pbuf,count);
    if(ret == 0)
    {
        printk("APP write data:%s\n",writebuf);
        printk("kernel write count:%d!\n",count);
        printk("chrdevbase_write success!\n");
    }else
    {
        printk("kernel recv data error!\n");
    }
    return 0;
}
ssize_t chrdevbase_read(struct file *filp, char __user *pbuf, size_t count, loff_t *offt)
{
    int ret = 0;
    memcpy(readbuf,kerneldata,sizeof(kerneldata));
    ret = copy_to_user(pbuf,readbuf,count);
    if(ret == 0)
    {
        printk("kernel read count:%d!\n",count);
        printk("chrdevbase_read success!\n");
    }else
    {
        printk("kernel send data error!\n");
    }
    return 0;
}
int chrdevbase_close(struct inode *pinode, struct file *filp)
{
    printk("chrdevbase_close success!\n");
    return 0;
}

const struct file_operations chrdevbase_fops = {
    .owner = THIS_MODULE,
    .open = chrdevbase_open,
    .write = chrdevbase_write,
    .read = chrdevbase_read,
    .release = chrdevbase_close
};

static int __init chrdevbase_init(void)
{
    int ret = 0;
    ret = register_chrdev(CHRDEVBASE_MAJOR,CHRDEVBASE_NAME,&chrdevbase_fops);
    if(ret < 0)
        printk("chrdevbase_init error!\n");
    else
        printk("chrdevbase_init success!\n");
    return 0;
}
static void __exit chrdevbase_exit(void)
{
    unregister_chrdev(CHRDEVBASE_MAJOR,CHRDEVBASE_NAME);
    printk("chrdevbase_exit success!\n");
}

module_init(chrdevbase_init);
module_exit(chrdevbase_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("dong");