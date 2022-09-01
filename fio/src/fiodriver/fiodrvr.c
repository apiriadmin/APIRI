/*
 * fiodrvr.c - fiodriver device file
 *
 * Copyright (C) 2019 Intelight Inc. <support@intelight-its.com>
 * 
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */


#include <linux/module.h>
#include <linux/init.h>
#include <linux/cdev.h>
#include <linux/platform_device.h>
#include <linux/of_platform.h>
#include <linux/of_gpio.h>
#include <linux/gpio.h>

#include "fiodriver.h" /* FIO Driver Definitions */
#include "fioman.h"

dev_t fio_dev; /* Major / Minor */
struct cdev fio_cdev; /* character device */

int faultmon_gpio = -1;


static long fio_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	return fioman_ioctl(filp, cmd, arg);
}

static int fio_open(struct inode *inode, struct file *filp)
{
	return fioman_open(inode, filp);
}

static int fio_release(struct inode *inode, struct file *filp)
{
	return fioman_release(inode, filp);
}

/*
 * Module stuff
 */
static struct class *fio_class;

static char *fio_devnode(struct device *dev, umode_t *mode)
{
	return kasprintf(GFP_KERNEL, "%s", dev_name(dev));
}


static const struct file_operations fio_fops = {
	.owner          = THIS_MODULE,
	.open           = fio_open,
	.release        = fio_release,
	.unlocked_ioctl = fio_ioctl,
};

static int fio_probe(struct platform_device *pdev)
{
	struct device_node *np = pdev->dev.of_node;
	int gpio;

	gpio = of_get_gpio(np, 0); // NEMA controller type GPIO
	if (gpio_is_valid(gpio)
		&& (gpio_request_one(gpio, GPIOF_DIR_IN, "fiodriver") == 0)
		&& !gpio_get_value(gpio)) {
		// NEMA controller type is TS2-1
		faultmon_gpio = of_get_gpio(np, 1);
		if (!gpio_is_valid(faultmon_gpio)) {
			faultmon_gpio = -1;
			dev_err(&pdev->dev, "faultmon gpio not valid\n");
		} else if (gpio_request(faultmon_gpio, "fiodriver") != 0) {
			faultmon_gpio = -1;
			dev_err(&pdev->dev, "faultmon gpio request failed\n");
		} else {
			gpio_direction_output(faultmon_gpio, 0); /* initially OFF */
			if (gpio_cansleep(faultmon_gpio))
				gpio_set_value_cansleep(faultmon_gpio, 0);
			else
				gpio_set_value(faultmon_gpio, 0);
			dev_info(&pdev->dev, "faultmon gpio enabled\n");
		}
	}
	
	fioman_init();
	
	printk( KERN_ALERT "FIO driver loaded\n" );

	return 0;
}

static int fio_remove(struct platform_device *pdev)
{

	gpio_free(faultmon_gpio);

	dev_set_drvdata(&pdev->dev, NULL);

	fioman_exit();

	printk( KERN_ALERT "FIO Driver Unloaded\n" );

	return 0;
}

static const struct of_device_id of_fiodriver_match[] = {
	{ .compatible = "linux,fiodriver", },
	{},
};

MODULE_DEVICE_TABLE(of, of_fiodriver_match);

static struct platform_driver fio_driver = {
	.driver = {
		.name = "fiodriver",
		.owner = THIS_MODULE,
		.of_match_table = of_fiodriver_match,
	},
	.probe = 	fio_probe,
	.remove = 	fio_remove,
};

static int __init fio_init(void)
{
	int ret;

	/* Allocate major number */
	ret = alloc_chrdev_region( &fio_dev, 0, 1, "fiodriver" );
	if (ret) {
		pr_err("fiodriver: cannot allocate major number for chrdev\n" );
		goto error;
	}

	/* Set up the character device and let it be know by the kernel */
	cdev_init( &fio_cdev, &fio_fops );
	fio_cdev.owner = THIS_MODULE;
	fio_cdev.ops = &fio_fops;
	ret = cdev_add( &fio_cdev, fio_dev, 1 );
	if (ret) {
		printk( KERN_ALERT "FIO Driver cannot be added\n" );
		goto error_region;
	}

	/* Create a class for this device and add the device */
	fio_class = class_create(THIS_MODULE, "fio");
	if (IS_ERR(fio_class)) {
		printk(KERN_ERR "Error creating fio class.\n");
		cdev_del(&fio_cdev);
		ret = PTR_ERR(fio_class);
		goto error_region;
	}
	fio_class->devnode = fio_devnode;

	device_create(fio_class, NULL, fio_dev, NULL, "fio");

	return platform_driver_register(&fio_driver);

error_region:
	unregister_chrdev_region(fio_dev, 1);
error:
	return ret;
}

static void __exit fio_exit(void)
{
	platform_driver_unregister(&fio_driver);
	class_destroy(fio_class);
	unregister_chrdev_region(fio_dev, 1);
}

module_init(fio_init);
module_exit(fio_exit);

MODULE_AUTHOR("J. Michael Gallagher, Intelight Inc. <support@intelight-its.com>");
MODULE_DESCRIPTION( "FIO API Module for ATC" );
MODULE_VERSION( "03.06" );
MODULE_LICENSE("GPL");
