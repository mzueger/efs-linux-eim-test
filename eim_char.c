#include <linux/init.h>
#include <linux/module.h>
#include <linux/io.h>
#include <linux/uaccess.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/cdev.h>
#include <linux/fs.h>
#include <linux/platform_device.h>
#include <linux/of_device.h>
#include <linux/of_address.h>

#include "eim_char.h"

#define MODULE_NAME "eim-char"

static dev_t           dev;
static struct cdev*    eim_char_dev;
static struct class*   eim_class;
static atomic_t        eim_available = ATOMIC_INIT(1);
static struct resource res;
static void __iomem*   eim_base_ptr;

/* helper functions */

// static inline int write32(void __iomem* addr, u32 val) {
// 	iowrite32(val, addr);
// 	return 0;
// }

/* file operations */

static int open(struct inode* ind, struct file* flp) {
	if(!atomic_dec_and_test(&eim_available)) {
		atomic_inc(&eim_available);
		return -EBUSY; /* device already open */
	}
	return 0;
}

static int close(struct inode* ind, struct file* flp) {
	atomic_inc(&eim_available); /* release the device */
	return 0;
}

static ssize_t read(struct file* flp, char __user* data, size_t size, loff_t* offset) {
	u32 buf;
	size_t i = 0;
	u8* read_ptr = (u8*)eim_base_ptr;
	
	if(size % 4 != 0) return 0;
	
	while(i < size) {
		buf = ioread32(read_ptr + i);
		copy_to_user(&data + i, &buf, 4);
		i += 4;
	}
	
	return i;
}

static ssize_t write(struct file* flp, const char __user* data, size_t size, loff_t* offset) {
	u32 buf;
	size_t i = 0;
	int rest = size % 4;
	u8* write_ptr = (u8*)eim_base_ptr;
	
	while(i + 4 <= size) {
		copy_from_user(&buf, data + i, 4);
		iowrite32(buf, write_ptr + i);
		i += 4;
	}
	if(rest > 0) {
		buf = 0;
		copy_from_user(&buf, data + i, rest);
		iowrite32(buf, write_ptr + i);
	}
	return i;
}

struct file_operations eim_fops = {
	.owner = THIS_MODULE,
	.open = open,
	.release = close,
	.read = read,
	.write = write,
};

/* initialization and cleanup */

static struct of_device_id eim_char_of_match[] = {
	{ .compatible = "ntb,eim-char", },
	{ }
};

MODULE_DEVICE_TABLE(of, eim_char_of_match);

static int eim_char_probe(struct platform_device* pdev) {
	int error = 0;
	const struct of_device_id* match = of_match_device(eim_char_of_match, &pdev->dev);
	
	/* sanity check */
	if(!match) goto match_failure;
	
	/* get resource from dt */
	error = of_address_to_resource(pdev->dev.of_node, 0, &res);
	if(error) goto ressource_failure;
	
	/* request memory region */
	if(!request_mem_region(res.start, resource_size(&res), "eim-char")) goto mem_request_failure;
	
	/* remap  memory region */
	eim_base_ptr = of_iomap(pdev->dev.of_node, 0);
	if(!eim_base_ptr) goto mem_iomap_failure;
	
	/* create cdev */
	error = alloc_chrdev_region(&dev, 0, 1, MODULE_NAME);
	if(error) goto alloc_chrdev_region_failure;
	
	eim_char_dev = cdev_alloc();
	eim_char_dev->owner = THIS_MODULE;
	eim_char_dev->ops = &eim_fops;
	
	error = cdev_add(eim_char_dev, dev,1);
	if(error) goto cdev_add_failure;
	
	/* create device class */
	eim_class = class_create(THIS_MODULE, MODULE_NAME);
	device_create(eim_class, NULL, dev, NULL, "eim-char%d", 0);
	
	return 0;
	
	/* error handling */
	cdev_add_failure:
		device_destroy(eim_class, dev);
		cdev_del(eim_char_dev);
		unregister_chrdev_region(dev, 1);
	alloc_chrdev_region_failure:
		iounmap(eim_base_ptr);
	mem_iomap_failure:
		release_mem_region(res.start, resource_size(&res));
	mem_request_failure:
		// nothing to cleanup
	ressource_failure:
		// nothing to cleanup
	match_failure:
		// nothing to cleanup
	
	printk(KERN_ALERT "Error while initializing module (%s)!\n", MODULE_NAME);
	return error;
}

static int eim_char_remove(struct platform_device* pdev) {
	device_destroy(eim_class, dev);
	class_destroy(eim_class);
	cdev_del(eim_char_dev);
	unregister_chrdev_region(dev, 1);
	iounmap(eim_base_ptr);
	release_mem_region(res.start, resource_size(&res));
	return 0;
}

static struct platform_driver eim_char_platform_driver = {
	.probe = eim_char_probe,
	.remove = eim_char_remove,
	.driver = {
		.name = "eim-char",
		.owner = THIS_MODULE,
		.of_match_table = eim_char_of_match,
  },
};

static int __init eim_char_init(void) {
	return platform_driver_register(&eim_char_platform_driver);
}

static void __exit eim_char_cleanup(void) {
	platform_driver_unregister(&eim_char_platform_driver);
}

module_init(eim_char_init);
module_exit(eim_char_cleanup);

MODULE_AUTHOR("Martin Zueger <martin.zueger@ntb.ch");
MODULE_DESCRIPTION("EIM bus test driver for i.MX6 based boards");
MODULE_LICENSE("GPL");
