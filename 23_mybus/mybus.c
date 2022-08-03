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

struct mybus_driver{
    int drv_version;
    struct device_driver drv;
    int (*probe)(struct mybus_device *dev);
};
struct mybus_device{
    int dev_version;
    struct device dev;
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

void dev_release(struct device *dev)
{
    printk("dev_release success!\n");
}
int mybusdrv_probe(struct mybus_device *dev)
{
    printk("mybusdrv_probe success!\n");
    return 0;
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

struct mybus_driver mybusdrv = {
    .drv_version = 0x1122,
    .drv = {
        .name = "mybusdrv",
        .bus = &mybus,
        .probe = drv_probe
    },
    .probe = mybusdrv_probe
};
struct mybus_device mybusdev = {
    .dev_version = 0x1122,
    .dev = {
        .init_name = "mybusdev",
        .bus = &mybus,
        .release = dev_release
    }
};

static int __init mybus_init(void)
{
    int ret = 0;
    ret = bus_register(&mybus);
    ret = driver_register(&mybusdrv.drv);
    ret = device_register(&mybusdev.dev);
    return 0;
}
static void __exit mybus_exit(void)
{
    driver_unregister(&mybusdrv.drv);
    device_unregister(&mybusdev.dev);
    bus_unregister(&mybus);
}

module_init(mybus_init);
module_exit(mybus_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("dong");