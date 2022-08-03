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

#define  NODE_PATH    "/backlight"
#define  PROP_NAME1   "compatible"
#define  PROP_NAME2   "status"
#define  PROP_NAME3   "default-brightness-level"
#define  PROP_NAME4   "brightness-levels"

struct dtsof_node{
    struct device_node *dn;
    struct property *prop;
    const char *strval[1];
    u32 intval;
    int elem_size;
    u32 arrval[8];
    int i;
};
struct dtsof_node *dtsof;

static int __init dtsof_init(void)
{
    int ret = 0;
    dtsof = (struct dtsof_node *)kmalloc(sizeof(struct dtsof_node),GFP_KERNEL);
    if(!dtsof)
    {
        printk("kmalloc error!\n");
        return -EINVAL;
    }

    dtsof->dn = of_find_node_by_path(NODE_PATH);
    if(!dtsof->dn)
    {
        printk("of_find_node_by_path error!\n");
        ret = -EINVAL;
        goto find_node_error;
    }
    printk("node:%s!\n",dtsof->dn->name);

    dtsof->prop = of_find_property(dtsof->dn,PROP_NAME1,NULL);
    if(!dtsof->prop)
    {
        printk("of_find_property error!\n");
        ret = -EINVAL;
        goto find_prop1_error;
    }
    printk("%s:%s!\n",dtsof->prop->name,(char *)dtsof->prop->value);

    ret = of_property_read_string(dtsof->dn,PROP_NAME2,dtsof->strval);
    if(ret != 0)
    {
        printk("of_property_read_string error!\n");
        goto find_prop2_error;
    }
    printk("status:%s!\n",dtsof->strval[0]);

    ret = of_property_read_u32(dtsof->dn,PROP_NAME3,&dtsof->intval);
    if(ret != 0)
    {
        printk("of_property_read_u32 error!\n");
        goto find_prop3_error;
    }
    printk("default-brightness-level:%d!\n",dtsof->intval);

    dtsof->elem_size = of_property_count_elems_of_size(dtsof->dn,PROP_NAME4,sizeof(u32));
    if(dtsof->elem_size < 0)
    {
        printk("of_property_count_elems_of_size error!\n");
        ret = dtsof->elem_size;
        goto count_elems_error;
    }
    printk("brightness-levels count elems:%d!\n",dtsof->elem_size);

    ret = of_property_read_u32_array(dtsof->dn,PROP_NAME4,dtsof->arrval,dtsof->elem_size);
    if(ret != 0)
    {
        printk("of_property_read_u32_array error!\n");
        goto find_prop4_error;
    }
    for(dtsof->i = 0; dtsof->i < dtsof->elem_size; dtsof->i++)
    {
        printk("brightness-levels[%d]:%d!\n",dtsof->i,dtsof->arrval[dtsof->i]);
    }
    return 0;

    find_prop4_error:
    count_elems_error:
    find_prop3_error:
    find_prop2_error:
    find_prop1_error:
    find_node_error:
        kfree(dtsof);
    return ret;
}
static void __exit dtsof_exit(void)
{
    kfree(dtsof);
}

module_init(dtsof_init);
module_exit(dtsof_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("dong");