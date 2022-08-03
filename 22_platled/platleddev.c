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

#define CCM_CCGR1_PBASE             0x020c406c
#define SW_MUX_GPIO1_IO03_PBASE     0x020e0068
#define SW_PAD_GPIO1_IO03_PBASE     0x020e02f4
#define GPIO1_DR_PBASE              0x0209c000
#define GPIO1_GDIR_PBASE            0x0209c004

#define LED_REG_LENGTH              4

typedef struct platled_prvdata_struct{
    int ccm_ccgr1_data;
    int ccm_ccgr1_shift;
    int sw_mux_gpio1_io03_data;
    int sw_pad_gpio1_io03_data;
    int gpio1_dr_pos;
    int gpio1_gdir_pos;
}plpd;
plpd platled_prvdata = {
    .ccm_ccgr1_data = 3,
    .ccm_ccgr1_shift = 26,
    .sw_mux_gpio1_io03_data = 5,
    .sw_pad_gpio1_io03_data = 0x10b0,
    .gpio1_dr_pos = 8,
    .gpio1_gdir_pos = 8
};

void platleddev_release(struct device *dev)
{
    printk("release success!\n");
}

struct resource platleddev_resource[] = {
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

struct platform_device platleddev = {
    .name = "platled",
    .id = -1,
    .dev = {
        .platform_data = &platled_prvdata,
        .release = platleddev_release
    },
    .num_resources = ARRAY_SIZE(platleddev_resource),
    .resource = platleddev_resource
};

static int __init platleddev_init(void)
{
    return platform_device_register(&platleddev);
}
static void __exit platleddev_exit(void)
{
    platform_device_unregister(&platleddev);
}

module_init(platleddev_init);
module_exit(platleddev_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("dong");