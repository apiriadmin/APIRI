/*
 * Copyright 2014, 2015 AASHTO/ITE/NEMA.
 * American Association of State Highway and Transportation Officials,
 * Institute of Transportation Engineers and
 * National Electrical Manufacturers Association.
 *  
 * This file is part of the Advanced Transportation Controller (ATC)
 * Application Programming Interface Reference Implementation (APIRI).
 *  
 * The APIRI is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation, either version 2.1 of the
 * License, or (at your option) any later version.
 *  
 * The APIRI is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *  
 * You should have received a copy of the GNU Lesser General Public
 * License along with the APIRI.  If not, see <http://www.gnu.org/licenses/>.
 */

/*****************************************************************************/
/*

This module combines all the pieces of the FIO driver needed to compile a
kernel module.

*/
/*****************************************************************************/

/*  Include section.
-----------------------------------------------------------------------------*/
/* System includes. */

#include	<linux/module.h>	/* Module definitions */
#include	<linux/init.h>		/* Module init definitions */
#include	<linux/sched.h>		/* Scheduler definitions */
#include	<linux/kthread.h>	/* Kernel thread definitions */
#include	<linux/list.h>		/* List definitions */
#include	<linux/fs.h>		/* File structure definitions */
#include	<linux/cdev.h>		/* Character driver definitions */
#include	<linux/device.h>
#include        <linux/version.h>

/* Local includes. */
#include	"fiodriver.h"				/* FIO Driver Definitions */
#include	"fioman.h"

/*  Definition section.
-----------------------------------------------------------------------------*/

/*  Global section.
-----------------------------------------------------------------------------*/
/* Inlined C files */
/*#include	"fioman.c"*/		/* FIOMAN Code */

dev_t			fio_dev;		/* Major / Minor */
struct cdev		fio_cdev;		/* character device */

/*  Private API declaration (prototype) section.
-----------------------------------------------------------------------------*/

/*  Public API interface section.
-----------------------------------------------------------------------------*/

struct file_operations	fio_fops;

/*  Private API implementation section.
-----------------------------------------------------------------------------*/

#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,31)
static struct class *fio_class;

static char *fio_devnode(struct device *dev, mode_t *mode)
{
	return kasprintf(GFP_KERNEL, "%s", dev_name(dev));
	//return "fio";
}
#endif
/*****************************************************************************/
/*
This function initializes the FIO Driver module.
*/
/*****************************************************************************/

static int __init
fio_init( void )
{
	int ret;

	/* Allocate my major number */
	ret = alloc_chrdev_region( &fio_dev, 0, 1, "fiodriver" );
	if ( ret ) {
		printk( KERN_ALERT "Cannot allocate major for fiodriver\n" );
		goto error;
	}

	/*-------------------------------------------------------------------*/

	/* FIOMAN INITIALIZATION */

	fioman_init();

	/*-------------------------------------------------------------------*/

	/* Set up the character device and let it be know by the kernel */
	cdev_init( &fio_cdev, &fio_fops );
	fio_cdev.owner = THIS_MODULE;
	fio_cdev.ops = &fio_fops;
	ret = cdev_add( &fio_cdev, fio_dev, 1 );
	if ( ret ) {
		printk( KERN_ALERT "FIO Driver cannot be added\n" );
		goto error_region;
	}

#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,31)
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
#endif
	/* DRIVER IS LIVE */

	printk( KERN_ALERT "FIO Driver Loaded\n" );
	return 0;

error_region:
	unregister_chrdev_region(fio_dev, 1);
	/* Clean up FIOMAN */
	fioman_exit();
error:
	return ret;
}

/*****************************************************************************/

/*****************************************************************************/
/*
This function cleans up the FIO Driver Module for unloading
*/
/*****************************************************************************/

static void __exit
fio_exit( void )
{
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,31)
	device_destroy(fio_class, fio_dev);
	class_destroy(fio_class);
#endif
	/* Make character driver unavailable */
	cdev_del( &fio_cdev );

	/* Unregister major / minor */
	unregister_chrdev_region( fio_dev, 1 );

	/* Clean up FIOMAN */
	fioman_exit();

	printk( KERN_ALERT "FIO Driver Unloaded\n" );
}

/*****************************************************************************/

/*  Public API implementation section.
-----------------------------------------------------------------------------*/

/*****************************************************************************/
/*
This function is used to open and initialize the FIO Driver
*/
/*****************************************************************************/

int
fio_open
(
	struct inode	*inode,		/* INode of open */
	struct file	*filp		/* File structure for open */
)
{
	/* Let FIOMAN do its stuff */
	return ( fioman_open( inode, filp ) );
}

/*****************************************************************************/

/*****************************************************************************/
/*
This function is used to close and cleanup the FIO Driver
*/
/*****************************************************************************/

int
fio_release
(
	struct inode	*inode,		/* INode of open */
	struct file		*filp		/* File structure for open */
)
{
	/* Let FIOMAN do its stuff */
	return ( fioman_release( inode, filp ) );
}

/*****************************************************************************/

/*****************************************************************************/
/*
This function is the ioctl interface for the FIO psuedo driver.
*/
/*****************************************************************************/

long
fio_ioctl
(
	struct file	*filp,	/* File Pointer */
	unsigned int	cmd,	/* Command associated with this call */
	unsigned long	arg	/* Argument for this command */
)
{
	/* Let FIOMAN do its stuff */
	return ( fioman_ioctl( filp, cmd, arg ) );
}

/*****************************************************************************/

/* File operations */
struct file_operations	fio_fops =
{
	.owner =		THIS_MODULE,
	.open =			fio_open,
	.release =		fio_release,
	.unlocked_ioctl =	fio_ioctl,
};

module_init( fio_init );	/* Driver initialization */
module_exit( fio_exit );	/* Driver cleanup */

/*****************************************************************************/
/*

REVISION HISTORY:
$Log$

*/
/*****************************************************************************/
MODULE_AUTHOR( "Thomas E. Gauger tgauger@vanteon.com" );
MODULE_DESCRIPTION( "FIO API Module for ATC" );
MODULE_VERSION( "01.00" );
MODULE_LICENSE("GPL");

