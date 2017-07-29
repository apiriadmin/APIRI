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

#include <linux/module.h>
//#include <linux/config.h>

#include <linux/fs.h>
#include <linux/errno.h>
#include <linux/miscdevice.h>
#include <linux/kernel.h>
#include <linux/major.h>
#include <linux/slab.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
//#include <linux/devfs_fs_kernel.h>
#include <linux/stat.h>
#include <linux/init.h>
#include <linux/poll.h>
#include <linux/device.h>
#include <linux/tty.h>
#include <linux/kmod.h>
#include <linux/cdev.h>
#include <linux/signal.h>
#include <linux/sched.h>

#include "front_panel.h"

//#define CONFIG_FP_PROC_FS
// #define CONFIG_FP_DEV_FS

#define FP_VERSION_STR "Intelight, 2.01, 2.17"



#ifdef CONFIG_FP_PROC_FS
#include <linux/proc_fs.h>
int fp_read_procmem(char *buf, char **start, off_t offset, int count, int *eof, void *data);
#endif

#ifndef LINUX_VERSION_CODE
#include <linux/version.h>
#endif				/* LINUX_VERSION_CODE */

// dynamically assign a major number to this driver
int fp_major;

#define MAX(a,b)	((a>b)?a:b)
#define MIN(a,b)	((a<b)?a:b)
#define ABS(a)		((a<0)?-a:a)


//
// this structure add linking to the read_packet structure
// so that is can be added to lists
//
typedef struct list_packet {
	struct list_head     links;		// linked list pointers
	struct read_packet   packet;		// the actual data payload
} list_packet;


//
// this structure holds necessary information for each
// interface that is currently active.
//
typedef struct fp_device_s {                      /* holds the state of each active front panel device */
	wait_queue_head_t  wait;		/* the wait queue for blocking i/o on this interface */
	struct semaphore   sem;                 /* mutual exclusion semaphore     */
	struct cdev	   cdev;		/* Char device structure */
	struct list_head   vroot;               /* root of the read queue for each interface */
	struct list_head   droot;               /* root of the read queue for each interface */
	list_packet      * active_record;	/* reference to the current record being read (NULL when complete) */
	int		   pid;			/* the Id of the process that owns this interface */
	int		   slot;		/* a quick reference index back into the device table */
	int		   last_slot;		/* the most recent slot used by the same process that is using this slot. */
	enum { CLOSED=0x00, VR_OPEN=0x01, VW_OPEN=0x02, DR_OPEN=0x04, DW_OPEN=0x08 } open_flags;
	char               fp_debug;            /* 0->off, 1->sense, 9->dump dev, 10-> all devs */
	int		   aux_state;		/* used by AUX Switch interface to store it's current state */
} fp_device_t;

#define V_MASK	(0x03)
#define D_MASK	(0x0c)

// static fp_device_t *fp_devtab[ FP_MAX_DEVS ];	// statically allocate an array of pointers
static fp_device_t fp_devtab[FP_MAX_DEVS];		// statically allocate an array of structures


int            fp_open(struct inode *inode, struct file *filp);
int            fp_release(struct inode *inode, struct file *filp);
ssize_t        fp_read(struct file *filp, char __user *buf, size_t count, loff_t * fpos);
ssize_t        fp_write(struct file *filp, const char __user *buf, size_t count, loff_t * fpos);
long           fp_ioctl(struct file *filp, unsigned int cmd_in, unsigned long arg);
unsigned int   fp_poll(struct file *filp, poll_table * wait);
void	       destroy_writer( fp_device_t * );

list_packet  * send_packet( int command, int from, int to, const char __user *buf, size_t count );

//
// these are the file ops for the different interfaces
//
struct file_operations fp_fops = {
	.owner   = THIS_MODULE,
	.open    = fp_open,
	.release = fp_release,
	.read    = fp_read,
	.write   = fp_write,
	.llseek  = no_llseek,
	.poll    = fp_poll,
	.unlocked_ioctl   = fp_ioctl,
};

struct file_operations fp_ro_fops = {
	.owner   = THIS_MODULE,
	.open    = fp_open,
	.release = fp_release,
	.read    = fp_read,
	.llseek  = no_llseek,
	.poll    = fp_poll,
	.unlocked_ioctl   = fp_ioctl,
};

struct file_operations fp_wo_fops = {
	.owner   = THIS_MODULE,
	.open    = fp_open,
	.release = fp_release,
	.write   = fp_write,
	.llseek  = no_llseek,
	.poll    = fp_poll,
	.unlocked_ioctl   = fp_ioctl,
};

struct file_operations aux_fops = {
	.owner   = THIS_MODULE,
	.open    = fp_open,
	.release = fp_release,
	.read    = fp_read,
	.llseek  = no_llseek,
	.poll    = fp_poll,
};



//
// this is a list of the device nodes we need to create at boot
// minor, name, permissions, file ops
//
static struct {
        unsigned int            minor;
	char                    *name;
	umode_t                 mode;
	struct file_operations  *fops;
	struct cdev		*cdev;
} devlist[] = { /* list of minor devices */
	{ 0,   "fpi",  S_IRUGO | S_IWUGO, &fp_fops,  NULL },
	{ 1,   "msi",  S_IRUSR | S_IWUSR, &fp_fops,  NULL },
	{ 2,   "sci",  S_IRUSR | S_IWUSR, &fp_fops,  NULL },
	{ 3,   "scm",  S_IRUSR | S_IWUSR, &fp_fops,  NULL },
	{ 4,   "aux",  S_IRUGO          , &aux_fops, NULL },
	{ 8,   "fpm",  S_IRUSR | S_IWUSR, &fp_fops,  NULL }
};



MODULE_AUTHOR("Greg Huber");
MODULE_DESCRIPTION("Front Panel Driver");
MODULE_LICENSE("GPL");
MODULE_VERSION(FP_VERSION_STR);



static char sts_buf[8];
static char *slot_to_string( int slot )
{
	if( slot < 0 ) {
		return( "Underflow");
	} else if( slot < APP_OPENS ) {
		sprintf( sts_buf, "APP(%d)", slot );
		return( sts_buf );
	} else if( slot == MS_DEV ) {
		return( "MS_DEV" );
	} else if( slot == SCI_DEV ) {
		return( "SCI_DEV" );
	} else if( slot == SCM_DEV ) {
		return( "SCM_DEV" );
	} else if( slot == AUX_DEV ) {
		return( "AUX_DEV" );
	} else if( slot == FPM_DEV ) {
		return( "FPM_DEV" );
	}
	return( "Overflow" );
}

#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,31)
static struct class *fp_class;

#if LINUX_VERSION_CODE < KERNEL_VERSION(3,3,0)
static char *fp_devnode(struct device *dev, mode_t *mode)
#else
static char *fp_devnode(struct device *dev, umode_t *mode)
#endif
{
	return kasprintf(GFP_KERNEL, "%s", dev_name(dev));
}
#endif

static int __init fp_init(void)
{
	int i;	// working variable for counters etc.
	int devno;
	int result;

	// initialize the device table array
	for( i = 0; i < FP_MAX_DEVS; i++ ) {
		fp_devtab[i].pid  = -1;				// initialize the owner PID
		fp_devtab[i].slot = i;				// set the index of this device descriptor
		fp_devtab[i].open_flags = CLOSED;		// initialize the open flags
		sema_init( &fp_devtab[i].sem, 1 );		// initialize the device semaphore
		init_waitqueue_head( &fp_devtab[i].wait );	// initialize the wait queue for this device
		INIT_LIST_HEAD( &fp_devtab[i].vroot);		// initialize the payload root pointer
		INIT_LIST_HEAD( &fp_devtab[i].droot);		// initialize the (direct) payload root pointer
	}

	// register for a major number assignment
	// allocate a cdev structure  for each minor
	// /proc/devices will contain an entry "<major> fpm" for this device
	fp_major = FP_GENERIC_MAJOR;
	for( i=0; i < ARRAY_SIZE(devlist); i++ ) {
		if( fp_major ) {
			devno = MKDEV( fp_major, devlist[i].minor );
			result = register_chrdev_region( devno, 1, devlist[i].name );
		} else {
			result = alloc_chrdev_region( &devno, devlist[i].minor, 1, devlist[i].name );
			fp_major = MAJOR( devno );
		}

		if( result < 0 ) {
			printk( KERN_WARNING "%s: can't get a major %d\n", __func__, fp_major );
		}

		devlist[i].cdev = cdev_alloc();
		cdev_init( devlist[i].cdev, devlist[i].fops );
		devlist[i].cdev->owner = THIS_MODULE;
		devlist[i].cdev->ops   = devlist[i].fops;
		result = cdev_add( devlist[i].cdev, devno, 1 );
		if( result ) {
			printk( KERN_WARNING "%s: Error %d adding minor %d\n", __func__, result, devlist[i].minor );
		}

	}

	printk( KERN_ALERT "\n\nfront_panel loaded at major = %d\n", fp_major );

#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,31)
	fp_class = class_create(THIS_MODULE, "front-panel");
	if (IS_ERR(fp_class)) {
		printk(KERN_ERR "Error creating front-panel class.\n");
	}
	fp_class->devnode = fp_devnode;

	for (i = 0; i < ARRAY_SIZE(devlist); i++) {
		device_create(fp_class, NULL, MKDEV(fp_major, devlist[i].minor),
				NULL, devlist[i].name );
	}
#endif

#ifdef CONFIG_FP_DEV_FS
	// create a sub directory within the /dev directory
	fp_dir = devfs_mk_dir( NULL, "fps", NULL )
	if( !fp_dir )
		return -EBUSY;

	// finally, create the 6 device nodes
	for (i = 0; i < ARRAY_SIZE(devlist); i++) {
		devfs_register( fp_dir, devlist[i].name, DEVFS_FL_DEFAULT,
			fp_major, devlist[i].minor, S_IFCHR | devlist[i].mode,
			devlist[i].fops, devlist+i );

	}
#endif

#ifdef CONFIG_FP_PROC_FS
	/* register the proc entry */
	if(create_proc_read_entry("front_panel", 0, NULL, fp_read_procmem, NULL) == NULL)
	    {
		unregister_chrdev( fp_major, "fpm");
		printk( KERN_ALERT "front_panel failed to create proc entry\n" );
		return -ENOMEM;
	    }
#endif

	return 0;
}



static void __exit fp_exit(void)
{
	dev_t		   dev    = 0;
	int                i      = 0;
	fp_device_t      * fp_dev = NULL;
	list_packet      * lptr   = NULL;
	struct list_head * node   = NULL;
	struct list_head * next   = NULL;

#ifdef CONFIG_FP_PROC_FS
	/* deregister the proc entry */
	remove_proc_entry("front_panel", NULL /* parent dir */);
#endif	

	// initialize the device table array
	for( i = 0; i < FP_MAX_DEVS; i++ ) {
		fp_dev = &fp_devtab[i];
		fp_dev->pid = -1;

		// clean up any dangling read records
		list_for_each_safe( node, next, &fp_dev->vroot ) {
			lptr = container_of( node, list_packet, links );
			list_del( &lptr->links );			// unlink this structure from the list
			kfree( lptr ); 					// free this record
		}
	}

	for( i = 0; i < ARRAY_SIZE(devlist); i++ ) {
		dev = MKDEV( fp_major, devlist[i].minor );
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,31)
		device_destroy(fp_class, dev);
#endif
		cdev_del( devlist[i].cdev );
		kfree( devlist[i].cdev );
		unregister_chrdev_region( dev, 1 );
	}

#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,31)
	class_destroy(fp_class);
#endif

	printk(KERN_ALERT "front_panel unloaded\n" );
}



//
//	This driver supports 6 minor interfaces as follows:
//	minor 0 - general interface, limited to 16 opens.
//	minor 1 - Selection Manager interface, exclusive.
//	minor 2 - System Configuration Utility interface, exclusive.
//	minor 3 - System Configuration Manager, exclusive.
//	minor 4 - AUX Switch interface, exclusive read only.
//	minor 8 - Front Panel Manager, exclusive.
//
//	In general when an open is requested, the minor number is used to
// determine the class of device to open. The device descriptor table is
// then checked to make sure a device of this class is available. If an
// available descriptor is found, both the device descriptor and the file
// descriptor are updates to contain references to each other as well as
// other static information. In addition, the first three devices also
// require the creation of virtual terminals.
//
int fp_open(struct inode *inode, struct file *filp)
{
	fp_device_t *fp_dev = NULL, *fpd = NULL;
	int           devno     = iminor(inode);
	int	      slot      = 0;
	int	      this_slot = 0;
	int           flags     = filp->f_flags;
	int	      pid       = current->pid;

	nonseekable_open(inode, filp);
	printk("%s: minor=%d, filp=%p, f_flags=0x%x, f_op=%p, private_data=%p\n", 
		__func__, devno, filp, flags, filp->f_op, filp->private_data);

	switch( devno )
	    {
	    	case 0:		// General application interface
			/*
				1: if O_SYNC find the last slot used by this process that can have a direct interface
				2: if not O_SYNC or we did not find a previous slot, allocate a new slot
				3: initialize the slot
				4: update all slots for this process with the 
			*/
			slot = APP_OPENS;
			// if the O_SYNC flag is set, find the most recent slot used by this process
			if( (flags & O_SYNC) != 0 ) {
				for( slot = 0; slot < APP_OPENS; slot++ ) {
					fpd = &fp_devtab[slot];
					if( fpd->pid == pid ) {
						this_slot = fpd->last_slot;
						break;
					}
				}
			}

			// if this is a normal open or a unique direct open, grab a new slot.
			if( slot >= APP_OPENS ) {
				for( slot = 0; slot < APP_OPENS; slot++ ) {
					fpd = &fp_devtab[slot];
					if( fpd->open_flags == CLOSED ) {
						/*fp_dev->slot = slot;*/
						break;
					}
				}

				if( slot >= APP_OPENS )	 {
					return( -EMFILE );
				}
				// Create the virtual or direct mode terminal
				if( send_packet((flags & O_SYNC)?DIRECT:CREATE, slot, FPM_DEV, NULL, 0) == NULL) {
					return( -ENOMEM );
				}
				this_slot = slot;
			}

			fp_dev = &fp_devtab[this_slot];
			fp_dev->slot = this_slot;

			/*
				if the slot is new, set the flags as specified by the caller
				if the slot was previously opened, set the write flag as specified,
				but make sure that only one of the VR_OPEN or DR_OPEN flags gets set
			*/
			if( (flags & O_ACCMODE) == O_RDWR) {	// accmode = 0x02
				if( (flags & O_SYNC) == 0 ) {
					if( ((fp_dev->open_flags & VR_OPEN) != 0) ) {
						return( -EBUSY );
					} else {
						fp_dev->open_flags |= VR_OPEN;
					}

					if( (fp_dev->open_flags & VW_OPEN) != 0 ) {
						return( -EBUSY );
					} else {
						fp_dev->open_flags |= VW_OPEN;
					}
				} else {
					if( ((fp_dev->open_flags & DR_OPEN) != 0) ) {
						return( -EBUSY );
					} else {
						fp_dev->open_flags |= DR_OPEN;
					}

					if( (fp_dev->open_flags & DW_OPEN) != 0 ) {
						return( -EBUSY );
					} else {
						fp_dev->open_flags |= DW_OPEN;
					}
				}
			} else if( (flags & O_ACCMODE) == O_WRONLY) {	// accmode = 0x01
				if( (flags & O_SYNC) == 0 ) {
					if( (fp_dev->open_flags & VW_OPEN) != 0 ) {
						return( -EBUSY );
					} else {
						fp_dev->open_flags |= VW_OPEN;
					}
				} else {
					if( (fp_dev->open_flags & DW_OPEN) != 0 ) {
						return( -EBUSY );
					} else {
						fp_dev->open_flags |= DW_OPEN;
					}
				}
			} else if( (flags & O_ACCMODE) == O_RDONLY) {	// accmode = 0x00
				if( (flags & O_SYNC) == 0 ) {
					if( ((fp_dev->open_flags & VR_OPEN) != 0) ) {
						return( -EBUSY );
					} else {
						fp_dev->open_flags |= VR_OPEN;
					}
				} else {
					if( ((fp_dev->open_flags & DR_OPEN) != 0) ) {
						return( -EBUSY );
					} else {
						fp_dev->open_flags |= DR_OPEN;
					}
				}
			}

			// scan all slots and update the last_slot parameter for all slots owned by this process.
			// if the current slot can still be opened for direct access, mark all slots owned by the
			// current process with this slot number. If the slot has been opened for direct access,
			// mark all slots owned by the current process fully used. This will cause the next open
			// request by this process to find a new slot.
			if( (fp_dev->open_flags & D_MASK) != 0 ) {		// has a direct interface been assigned to this slot
				for( slot = 0; slot < APP_OPENS; slot++ ) {	// scan all slots
					fpd = &fp_devtab[slot];			// get a handle to the slot
					if( fpd->pid == pid ) {			// find slots owned by the current process
						fpd->last_slot = -1;		// mark the last slot value as fully used
					}
				}
			} else {						// otherwise the direct interface is available for this slot
				for( slot = 0; slot < APP_OPENS; slot++ ) {	// scan all slots
					fpd = &fp_devtab[slot];			// get a handle to the slot
					if( fpd->pid == pid ) {			// find slots owned by the current process
						fpd->last_slot = this_slot;	// update the last slot value for furure reference.
					}
				}
			}
			printk("%s: APP(%d) is open flags=0x%x %s\n",
				__func__, fp_dev->slot, flags, (flags&O_SYNC)?"(DIRECT)":"" );
			break;
	    	case 1:		// Master Selection interface
			fp_dev = &fp_devtab[MS_DEV];
			if( (flags & O_SYNC) != 0 ) {
				return( -EINVAL );
			} else if( fp_dev->open_flags != CLOSED ) {
				return( -EBUSY );
			} else if( (flags & O_EXCL) == 0 ) {
				return( -ENXIO );
			} else if( send_packet( CREATE, MS_DEV, FPM_DEV, NULL, 0 ) == NULL ) {
				return( -ENOMEM );
			} else {
				fp_dev->open_flags = VR_OPEN | VW_OPEN;
			}
			printk("%s: MS_DEV is open\n", __func__ );
			break;
	    	case 2:		// System Config Utility interface
			fp_dev = &fp_devtab[SCI_DEV];
			if( (flags & O_SYNC) != 0 ) {
				return( -EINVAL );
			} else if( fp_dev->open_flags != CLOSED ) {
				return( -EBUSY );
			} else if( (flags & O_EXCL) == 0 ) {
				return( -ENXIO );
			} else if( send_packet( CREATE, SCI_DEV, FPM_DEV, NULL, 0 ) == NULL ) {
				return( -ENOMEM );
			} else if( send_packet( CREATE, SCI_DEV, SCM_DEV, NULL, 0 ) == NULL ) {
				return( -ENOMEM );
			} else {
				fp_dev->open_flags = VR_OPEN | VW_OPEN;
			}
			printk("%s: SCI_DEV is open\n", __func__ );
			break;
	    	case 3:		// System Configuration Manager interface
			fp_dev = &fp_devtab[SCM_DEV];
			if( (flags & O_SYNC) != 0 ) {
				return( -EINVAL );
			} else if( fp_dev->open_flags != CLOSED ) {
				return( -EBUSY );
			} else if( (flags & O_EXCL) == 0 ) {
				return( -ENXIO );
			} else if( send_packet( CREATE, SCM_DEV, FPM_DEV, NULL, 0 ) == NULL ) {
				return( -ENOMEM );
			} else {
				fp_dev->open_flags = VR_OPEN | VW_OPEN;
			}
			printk("%s: SCM_DEV is open\n", __func__ );
			break;
	    	case 4:		// AUX Switch interface (read only)
			fp_dev = &fp_devtab[AUX_DEV];
			if( (flags & O_SYNC) != 0 ) {
				return( -EINVAL );
			} else if( fp_dev->open_flags != CLOSED ) {
				return( -EBUSY );
			} else if( (flags & O_ACCMODE) != O_RDONLY ) {
				return( -EACCES );
			} else {
				fp_dev->open_flags = VR_OPEN;
			}
			printk("%s: AUX_DEV is open\n", __func__ );
			break;
	    	case 8:		// Front Panel interface
			fp_dev = &fp_devtab[FPM_DEV];
			if( (flags & O_SYNC) != 0 ) {
				return( -EINVAL );
			} else if( fp_dev->open_flags != CLOSED ) {
				return( -EBUSY );
			} else if( (flags & O_EXCL) == 0 ) {
				return( -ENXIO );
			} else {
				fp_dev->open_flags = VR_OPEN | VW_OPEN;
			}
			printk("%s: FPM_DEV is open\n", __func__ );
			break;
		default:
			return( -ENXIO );
	    }

	fp_dev->pid = pid;				// save the pid of the task that opened this device for reading
	fp_dev->active_record = NULL;			// initialize the active_record reference pointer for the first fetch from the queues.
	filp->private_data = fp_dev;			// save a reference to this device descriptor in the File Descriptor

	pr_debug("%s: minor=%d, filp=%p, private_data=%p, slot=%s\n",
		__func__, devno, filp, filp->private_data, slot_to_string(fp_dev->slot));
	return( 0 );
}


/* Following function was formerly called 'fp_close' */
//
// This routine frees any remaining resources that the closing
// device may still be holding. The first three device types will
// have a virtual terminal associated with them that must be destroyed.
// After this, and for all devices, any pending read buffers are
// released and the device is detached.
int
fp_release(struct inode *inode, struct file *filp)
{
	fp_device_t   * fp_dev = (fp_device_t *)filp->private_data;
	int             devno  = iminor(inode);
	int             flags  = filp->f_flags;
	// int	        pid    = current->uid;
	int	        slot   = -1;

	if( fp_dev == NULL ) {
		return( -ENODEV );
	}

	slot = fp_dev->slot;

	printk("%s: Slot=%s, minor=%d, filp=%p, flags=0x%x, private_data=%p\n", 
		__func__, slot_to_string(slot), devno, filp, flags, filp->private_data );

	if( (flags & O_SYNC) == 0 ) {
		if( (flags & O_ACCMODE) == O_RDWR ) {
			if( (fp_dev->open_flags & VR_OPEN) == 0 ) {
				return( -EINVAL );
			} else {
				fp_dev->open_flags &= ~VR_OPEN;
			}

			if( (fp_dev->open_flags & VW_OPEN) == 0 ) {
				return( -EINVAL );
			} else {
				fp_dev->open_flags &= ~VW_OPEN;
			}
		} else if( (flags & O_ACCMODE) == O_WRONLY ) {
			if( (fp_dev->open_flags & VW_OPEN) == 0 ) {
				return( -EINVAL );
			} else {
				fp_dev->open_flags &= ~VW_OPEN;
			}
		} else if( (flags & O_ACCMODE) == O_RDONLY ) {
			if( (fp_dev->open_flags & VR_OPEN) == 0 ) {
				return( -EINVAL );
			} else {
				fp_dev->open_flags &= ~VR_OPEN;
			}
		}

		if( (fp_dev->open_flags & V_MASK) == 0 ) {
			list_packet      * lptr  = NULL;
			struct list_head * node  = NULL;
			struct list_head * next  = NULL;

			// clean up any dangling read records
			list_for_each_safe( node, next, &fp_dev->vroot ) {
				lptr = container_of( node, list_packet, links );
				list_del( &lptr->links );			// unlink this structure from the list
				kfree( lptr ); 					// free this record
			}
		}
	} else {
		if( (flags & O_ACCMODE) == O_RDWR ) {
			if( (fp_dev->open_flags & DR_OPEN) == 0 ) {
				return( -EINVAL );
			} else {
				fp_dev->open_flags &= ~DR_OPEN;
			}

			if( (fp_dev->open_flags & DW_OPEN) == 0 ) {
				return( -EINVAL );
			} else {
				fp_dev->open_flags &= ~DW_OPEN;
			}
		} else if( (flags & O_ACCMODE) == O_WRONLY ) {
			if( (fp_dev->open_flags & DW_OPEN) == 0 ) {
				return( -EINVAL );
			} else {
				fp_dev->open_flags &= ~DW_OPEN;
			}
		} else if( (flags & O_ACCMODE) == O_RDONLY ) {
			if( (fp_dev->open_flags & DR_OPEN) == 0 ) {
				return( -EINVAL );
			} else {
				fp_dev->open_flags &= ~DR_OPEN;
			}
		}

		if( (fp_dev->open_flags & D_MASK) == 0 ) {
			list_packet      * lptr  = NULL;
			struct list_head * node  = NULL;
			struct list_head * next  = NULL;


			// clean up any dangling read records
			list_for_each_safe( node, next, &fp_dev->droot ) {
				lptr = container_of( node, list_packet, links );
				list_del( &lptr->links );			// unlink this structure from the list
				kfree( lptr ); 					// free this record
			}
		}
	}

	// if this node is fully closed, remove the upstream resources also
	if( fp_dev->open_flags == CLOSED ) {
		switch( devno ) {
			case 0:
				send_packet( DESTROY, fp_dev->slot, MS_DEV,  NULL, 0 );	// remove the registration for this device
				send_packet( DESTROY, fp_dev->slot, FPM_DEV, NULL, 0 );	// tear down this devices virtual terminal
				break;
			case 2:
				send_packet( DESTROY, fp_dev->slot, FPM_DEV, NULL, 0 );	// tear down this devices virtual terminal
				send_packet( DESTROY, fp_dev->slot, SCM_DEV,  NULL, 0 );// notify SCM of Config Utility closure
				break;
			case 1:
			case 3:
				send_packet( DESTROY, fp_dev->slot, FPM_DEV, NULL, 0 );	// tear down this devices virtual terminal
				break;
			case 4:
			case 8:
				break;
			default:
				break;
		}
	}

	return 0;
}


//
// The state of the AUX Switch must be returned in a format compatable
// with the buffer the user has supplied. For now we simple copy AUX_ON
// or AUX_OFF to every byte of the users buffer.
//
int aux_state_to_user( fp_device_t * dev, char __user * buf, size_t count )
{
	size_t i;
	size_t res = 0;
	char token = (dev->aux_state)? AUX_SWITCH_ON : AUX_SWITCH_OFF;

	pr_debug("%s: Slot=%s, Aux=%d\n", __func__, slot_to_string(dev->slot), token );

	for( i = 0; i < count; i++ ) {
		res += copy_to_user( &buf[i], &token, 1 );
	}

	return( count );
}


//
// All reads are basically the same, with the exception of the AUX Switch.
// Generally, if data is pending on the read queue of the requesting interface
// it is copied back to the user and that record is popped off the queue.
// If the read queue is empty and O_NONBLOCK was set, EAGAIN is returned
// to indicate that no data is currently available. If O_NONBLOCK was not
// set, the process blocks until data becomes available.
//
// For the AUX Switch interface, if O_NONBLOCK was set, the current state
// of the switch is returned. If O_NONBLOCK is not set, the process blocks
// until there is a change in the state of the switch, at which time the
// new state is returned. 
//
ssize_t fp_read(struct file *filp, char __user *buf, size_t count, loff_t *fpos)
{
	fp_device_t	*fp_dev = (fp_device_t *)filp->private_data;
	read_packet	*rptr   = NULL;
	ssize_t		res    = 0;
	ssize_t         len    = 0;
	ssize_t		data_len = 0;
	char		*data_ptr = NULL;

	if( fp_dev == NULL ) {
		return( -ENODEV );
	}

	pr_debug("%s: Slot=%s, filp=%p, buf=%p, count=%ld, fpos=%ld\n",
		__func__, slot_to_string(fp_dev->slot), filp, buf, (long)count, (long)*fpos );

	if( count == 0 ) {							// if zero bytes requested, return zero bytes read
		return( 0 );
	} else if( count < 0 ) {						// if less than zero bytes requested, return an error
		return( -EINVAL );		
	}

	if( ! access_ok( VERIFY_WRITE, buf, count ) )				// verify that we can write to the users buffer
		return( -EFAULT );

	if( down_interruptible( &fp_dev->sem ) )				// protect access to the read queue
		return( -ERESTARTSYS );

	/*
		if active_record is NULL then we need to fetch a new record from the queue.
			if the queue has a record pending, pop it and load active_record.
			if the queue is empty and O_NONBLOCK was not set, block until a record is
				loaded into the queue then pop the record and load active_record.
			if the queue is empty and O_NONBLOCK is set, return EAGAIN and exit.
		if active_record is NOT NULL we have either a new record or one that was
			previously started but not completely consumed.
			Return the requested anount or the remainder (whichever is smaller) to the user
				update the pointers and exit.
	*/

	if( fp_dev->active_record == NULL ) {
		// we need a new record. Check the queue and handle the O_NONBLOCK flag as required.
		while( list_empty( &fp_dev->vroot ) ) {					// if the read queue is empty, we have a decision to make...

			up( &fp_dev->sem );						// allow access to the read queue

			if (filp->f_flags & O_NONBLOCK) {				// check the O_NONBLOCK flag
				res = -EAGAIN;						// most devices return EAGAIN if nonblocking

		        	if( fp_dev->slot == AUX_DEV ) {				// the AUX Switch returns its current state if nonblocking
			       		res = aux_state_to_user( fp_dev, buf, count );	// copy the AUX Switch's state to user space
				}

				return( res );						// return the result code ( or byte count )
		 	}

			pr_debug("%s: Slot %s blocked - going to sleep\n", __func__, slot_to_string(fp_dev->slot) );

			if( wait_event_interruptible( fp_dev->wait, !list_empty(&fp_dev->vroot) ) ) { // otherwise wait for something on the read queue
				//if( down_interruptible( &fp_dev->sem ) ) {		// protect access to the read queue for the next loop
				//	return( -ERESTARTSYS );
				//}
				// interrupted by signal
				return( -ERESTARTSYS );
			}

			pr_debug("%s: Slot %s unblocked\n", __func__, slot_to_string(fp_dev->slot) );

			if( down_interruptible( &fp_dev->sem ) ) {			// protect access to the read queue for the next loop
				return( -ERESTARTSYS );
			}
		}

		// NOTE: access to the queues is protected when we drop out of the while loop.

		// get the next entry off the desired list, pop the next entry off the other list
		// NOTE: we must always pop the associated record off the other queue in order to keep them in sync.
		//if( fp_dev->slot >= APP_OPENS ) {
			fp_dev->active_record = list_entry( fp_dev->vroot.next, list_packet, links );	// get the first record off the virtual list
			*fpos = 0;
		//} else if( ((filp->f_flags & O_SYNC) == 0) && ((fp_dev->open_flags & VR_OPEN) != 0) ) {
			//lptr = list_entry( fp_dev->droot.next, list_packet, links );	// get the first record off the direct list
			//list_del( &lptr->links );					// delete this entry
			//kfree( lptr );
		//	fp_dev->active_record = list_entry( fp_dev->vroot.next, list_packet, links );	// get the first record off the virtual list
		//	*fpos = 0;
		//} else if( ((filp->f_flags & O_SYNC) != 0) && ((fp_dev->open_flags & DR_OPEN) != 0) ) {
			//lptr = list_entry( fp_dev->vroot.next, list_packet, links );	// get the first record off the virtual list
			//list_del( &lptr->links );					// delete this entry
			//kfree( lptr );
		//	fp_dev->active_record = list_entry( fp_dev->droot.next, list_packet, links );	// get the first record off the direct list
		//	*fpos = 0;
		//} else {
			// ????? not open for read (O_WRONLY) ?????
			// return error
		//}
	}

	// NOTE: access to the queues is still protected either from the initial protect, or when a new record was fetched.

	rptr = &fp_dev->active_record->packet;						// get the read_packet out of the list_packet

	switch( fp_dev->slot ) {
		case FPM_DEV:	// this device will always read the entire record.
		case MS_DEV:	// this device will always read the entire record.
		case SCM_DEV:	// this device will always read the entire record.
			len = MIN( (rptr->size + sizeof( read_packet )), count );	// add in size of header and make sure it fits in users buffer
			pr_debug("%s: Slot=%s, reading from queue (transfer=%d bytes)\n", __func__, slot_to_string(fp_dev->slot), len );
			if( (res = copy_to_user( buf, rptr, len ) ) < 0 ) {		// copy the entire read_packet to the FP
				res = -EFAULT;
			} else {
				res = len - res;					// copy_to_user returns number of bytes NOT transfered
				*fpos = 0;						// clear the offset for the next read request
				list_del( &fp_dev->active_record->links );		// unlink this record from the list
				kfree( fp_dev->active_record );				// free this structure allocation
				fp_dev->active_record = NULL;				// clear the reference so we can fetch a new record
			}
			break;
		case AUX_DEV:	// return the last enqueued packet as latest AUX switch state
			res = aux_state_to_user( fp_dev, buf, count );
			break;
		default:	// the remaining devices may read only part of a record.
			data_len = rptr->size;
			data_ptr = rptr->data;

			// Access raw packet data for DIRECT mode
			if (filp->f_flags & O_SYNC) {
				data_len = rptr->size - rptr->raw_offset;
				data_ptr = &rptr->data[rptr->raw_offset];
			} else if (rptr->raw_offset) {
				data_len = rptr->raw_offset;
			}

			len = data_len - *fpos;					// calculate the number of bytes remaining to be transfered.
			if( (len < 0) || ( len > data_len ) ) {			// make sure the user has not underflowed or overflowed the buffer.
				res = -EIO;
				break;
			}

			len = MIN( len, count );					// make sure transfer fits in users buffer
			pr_debug("%s: Slot=%s, reading from other queue (transfer=%d bytes)\n", __func__, slot_to_string(fp_dev->slot), len );
			if( (res = copy_to_user( buf, &data_ptr[*fpos], len ) ) < 0 ) {	// copy the data to user space
				res = -EFAULT;
			} else {
				res = len - res;					// copy_to_user returns number of bytes NOT transfered
				*fpos += res;						// update the file offset for the next read request.
				if( *fpos  >= data_len ) {				// have we read the entire record??
					// Delete this record and any associated record.
					//if (filp->f_flags & O_SYNC) {
					//	if (!list_empty(&fp_dev->vroot)) {
					//		lptr = list_entry( fp_dev->vroot.next, list_packet, links );
					//		if (lptr->packet.sequence == rptr->sequence) {
					//			list_del( &lptr->links );
					//			kfree(lptr);
					//		}
					//	}
					//} else if (!list_empty(&fp_dev->droot)) {
					//	lptr = list_entry( fp_dev->droot.next, list_packet, links );
					//	if (lptr->packet.sequence == rptr->sequence) {
					//		list_del( &lptr->links );
					//		kfree(lptr);
					//	}
					//}

					list_del( &fp_dev->active_record->links );	// unlink this record from the list
					kfree( fp_dev->active_record );			// free this structure allocation
					fp_dev->active_record = NULL;			// clear the reference so we can fetch a new record
				}
			}
			break;
	}


	if( res >= 0 ) {
	}
	up( &fp_dev->sem );								// allow access to the read queue

	pr_debug("%s: Slot=%s exiting res=%d\n", __func__, slot_to_string(fp_dev->slot), res );
	return res;
}



ssize_t fp_write(struct file *filp, const char __user *buf, size_t count, loff_t * fpos)
{
	fp_device_t   * fp_dev = (fp_device_t *)filp->private_data;
	list_packet   * lptr   = NULL;
	ssize_t         res    = -EFAULT;

	if( fp_dev == NULL ) {								// verify that this device is open 
		return( -ENODEV );
	}

	pr_debug("%s: Slot=%s, filp=%p, buf=%p, count=%ld, fpos=%ld\n",
		__func__, slot_to_string(fp_dev->slot), filp, buf, (long)count, (long)*fpos );

	if( ! access_ok( VERIFY_READ, buf, count ) ) {					// verify that we can read from the users buffer
		return( -EFAULT );
	}

	//if( down_interruptible( &fp_dev->sem ) ) {
	//	return( -ERESTARTSYS );
	//}

	// command, from, to, buf, count
	// we need to find the source instance of this request in order to route it properly.
	switch( fp_dev->slot ) {
	    	case FPM_DEV:
			// these preformatted packets go to one of the application interfaces
			if( (lptr = send_packet( NOOP, FPM_DEV, 0, buf, count ) ) != NULL ) {
				res = lptr->packet.size;
			} else {
				res = -EFAULT;
			}
			pr_debug("%s: routing packet from %s to %s (size=%d, data=%p)\n", __func__,
				slot_to_string(lptr->packet.from), slot_to_string(lptr->packet.to), lptr->packet.size, lptr->packet.data );
			break;
		case AUX_DEV:	// this is an illegal operation.
			res = -EACCES;
			break;
		case -1:	// the requested interface was not found.
			res = -ENODEV;
			break;
		default:	// the remaining packets go to the Front Panel Manager
			if( (lptr = send_packet( DATA, fp_dev->slot, FPM_DEV, buf, count ) ) != NULL ) {
				res = lptr->packet.size;
			} else {
				res = -EFAULT;
			}
				
			break;
	}

	//up( &fp_dev->sem );

	if( lptr == NULL ) {
		return( -EFAULT );
	}

	return( res );
}



long
fp_ioctl(struct file *filp, unsigned int cmd_in, unsigned long arg)
{
	fp_device_t   * fp_dev  = (fp_device_t *)filp->private_data;
	int	        res     = 0;
	size_t	        len     = 0;
	char          * s       = NULL;
	char            ch      = '\0';

	pr_debug("%s: Slot=%d, filp=%p, cmd_in=%x, arg=%ld, private_data=%p\n",
		__func__, fp_dev->slot, filp, cmd_in, arg, filp->private_data );


	if( _IOC_TYPE( cmd_in ) != FP_IOC_MAGIC ) {
		pr_debug("%s: Bad Magic dir=%d, type=%d, nr=%d, size=%d\n", __func__,
			_IOC_DIR( cmd_in ), _IOC_TYPE( cmd_in ), _IOC_NR( cmd_in ), _IOC_SIZE( cmd_in ) );
		return( -EINVAL );
	}

	switch( cmd_in ) {
		case FP_IOC_REGISTER:
			pr_debug("%s: FP_IOC_REGISTER\n", __func__ );
			if( (fp_devtab[FPM_DEV].open_flags == CLOSED) || (fp_devtab[MS_DEV].open_flags == CLOSED) ) {
				printk("%s: FP_IOC_REGISTER - FPM_DEV(0x%x) or MS_DEV(0x%x) not available\n", __func__, fp_devtab[FPM_DEV].open_flags, fp_devtab[MS_DEV].open_flags );
				return( -EAGAIN );					// return error if not.
			} else if( (_IOC_DIR( cmd_in ) & _IOC_READ) != 0 ) {		// Check for an illegal read request
				return( -EFAULT );
			} else if( (fp_dev->slot < 0)  || (fp_dev->slot >= APP_OPENS) )	 { // Only Apps can issue this Ioctl
				return( -EFAULT );
			} else if( (_IOC_DIR( cmd_in ) & _IOC_WRITE) != 0 ) {		// check access to registration string
				if( !access_ok( VERIFY_READ, (void *)arg, _IOC_SIZE( cmd_in ) ) ) {
					return( -EFAULT );
				}
			}
			
			s = (char *)arg;						// cast a pointer to a character array
			for( len=0; ;len++ ) {						// start counting characters
				if( copy_from_user( &ch, &s[len], 1 ) != 0 ) {		// read the next character from the string
					return( -EFAULT );				// string ended short
				} else if( ch == '\0' ) {					// is this the NULL termination
					break;						// break out if end of line
				}
			}

			if( send_packet( REGISTER, fp_dev->slot, MS_DEV, s, len ) != NULL ) {
				res = 0;
			} else {
				res = -EFAULT;
			}
			break;
		case FP_IOC_SET_FOCUS:
			if( _IOC_DIR( cmd_in ) & _IOC_READ ) {				// Check for an illegal read request
				printk("%s: FP_IOC_SET_FOCUS bad IOC_DIR\n", __func__ );
				res = -EFAULT;
			} else if( (fp_dev->slot != MS_DEV) && (fp_dev->slot != SCM_DEV) ) {// Only Master Selection or System Config Mgr can issue this Ioctl
				printk("%s: FP_IOC_SET_FOCUS bad slot (not MS_DEV or SCM_DEV)\n", __func__ );
				res = -EFAULT;
			} else if( !access_ok( VERIFY_READ, (void *)arg, _IOC_SIZE( cmd_in ) ) ) {
				printk("%s: FP_IOC_SET_FOCUS bad argument\n", __func__ );
				res = -EFAULT;
			}
			if( res ) return( -EFAULT );

			pr_debug("%s: FP_IOC_SET_FOCUS\n", __func__ );
			if( send_packet( FOCUS, fp_dev->slot, FPM_DEV, (char *)arg, _IOC_SIZE( cmd_in ) ) != NULL ) {
				res = 0;
			} else {
				res = -EFAULT;
			}
			break;

		case FP_IOC_SETDEFAULT:
			pr_debug("%s: FP_IOC_SETDEFAULT\n", __func__ );
			break;
		case FP_IOC_VERSION:
			pr_debug("%s: FP_IOC_VERSION\n", __func__);
			if (copy_to_user((void __user *)arg, FP_VERSION_STR, strlen(FP_VERSION_STR)))
				res = -EFAULT;
			break;
		case FP_IOC_ATTRIBUTES:
			pr_debug("%s: FP_IOC_ATTRIBUTES\n", __func__);
#if 0
			if( send_packet( ATTRIBUTES, fp_dev->slot, FPM_DEV, (char *)arg, _IOC_SIZE( cmd_in ) ) != NULL ) {
				// block here to wait on FPM_DEV reply
// Maybe just call fp_read here?
				if( down_interruptible( &fp_dev->sem ) ) // protect access to the read queue
					return( -ERESTARTSYS );
				// wait for something on the read queue
				if( wait_event_interruptible( fp_dev->wait, !list_empty(&fp_dev->vroot) ) ) { 
					return( -ERESTARTSYS );
				}
				fp_dev->active_record = list_entry( fp_dev->vroot.next, list_packet, links );	// get the first record off the virtual list
				*fpos = 0;
				// copy from FPM_DEV reply msg to user
				if (copy_to_user((void __user *)arg, , sizeof(unsigned long)))
					res = -EFAULT;
				res = 0;
			} else {
				res = -EFAULT;
			}
#endif
			break;
		case FP_IOC_REFRESH:
			pr_debug("%s: FP_IOC_REFRESH\n", __func__ );
			if( fp_devtab[FPM_DEV].open_flags == CLOSED ){
				printk("%s: FP_IOC_REFRESH - FPM_DEV(0x%x) not available\n", __func__, fp_devtab[FPM_DEV].open_flags);
				return( -EAGAIN );					// return error if not.
			} else if( (_IOC_DIR( cmd_in ) & _IOC_READ) != 0 ) {		// Check for an illegal read request
				return( -EFAULT );
			} else if( (fp_dev->slot < 0)  || (fp_dev->slot >= APP_OPENS) )	 { // Only Apps can issue this Ioctl
				return( -EFAULT );
			}
			
			if( send_packet( REFRESH, fp_dev->slot, FPM_DEV, s, 0 ) != NULL ) {
				res = 0;
			} else {
				res = -EFAULT;
			}
			break;
		case FP_IOC_EMERGENCY:
			pr_debug("%s: FP_IOC_EMERGENCY\n", __func__ );
			if( fp_devtab[FPM_DEV].open_flags == CLOSED ){
				printk("%s: FP_IOC_EMERGENCY - FPM_DEV(0x%x) not available\n", __func__, fp_devtab[FPM_DEV].open_flags);
				return( -EAGAIN );					// return error if not.
			} else if( (_IOC_DIR( cmd_in ) & _IOC_READ) != 0 ) {		// Check for an illegal read request
				return( -EFAULT );
			} else if( (fp_dev->slot < 0)  || (fp_dev->slot >= APP_OPENS) )	 { // Only Apps can issue this Ioctl
				return( -EFAULT );
			}
			s = (char *)&arg;
			if( (send_packet(EMERGENCY, fp_dev->slot, FPM_DEV, (char *)&arg, _IOC_SIZE(cmd_in)) == NULL)
				|| (send_packet(EMERGENCY, fp_dev->slot, MS_DEV, s, _IOC_SIZE(cmd_in)) == NULL) ) {
				res = -EFAULT;
			}
			
			break;
			
		default:
			pr_debug("%s: Undefined Ioctl Command - 0x%x\n", __func__, cmd_in );
			res = -ENOTTY;
			break;
	}

	return( res );
}



//
// for poll, we can always write. We can read if there
// are buffers already in the queue
//
unsigned int
fp_poll( struct file *filp, poll_table * wait )
{
	fp_device_t   * fp_dev = (fp_device_t *)filp->private_data;
	int             flags  = filp->f_flags;
	unsigned int    mask   = 0;

	pr_debug("%s: filp=%p, wait=%p\n", __func__, filp, wait );

	if( fp_dev == NULL ) {
		return( POLLERR );
	}

	if( down_interruptible( &fp_dev->sem ) ) {
		return( -ERESTARTSYS );
	}

	poll_wait( filp, &fp_dev->wait, wait );


	if( (flags & O_SYNC) == 0 ) {

		mask = POLLOUT | POLLWRNORM;			// writes are always ready.

		if( ! list_empty( &fp_dev->vroot ) ) {		// check the read queue for pending packets
			mask |= POLLIN | POLLRDNORM;		// signal read data available
		}
	} else {
		// TODO need to check focus before setting this
		mask = POLLOUT | POLLWRNORM;			// writes are always ready.

		if( ! list_empty( &fp_dev->droot ) ) {		// check the read queue for pending packets
			mask |= POLLIN | POLLRDNORM;		// signal read data available
		}
	}

	up( &fp_dev->sem );

	return( mask );
}


//
// This routine will send the specified signal to the specified process
//
void send_signal( int pid, int sig )
{
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,26)
	struct task_struct * p = find_task_by_pid( pid );
#else
	struct task_struct * p = pid_task(find_vpid(pid), PIDTYPE_PID);
#endif
	send_sig_info( sig, SEND_SIG_PRIV, p );
}

//
// The FrontPanelManager writes preformatted packets.
// we just need to copy them into kernel space.
// Other devices will have a packet header created for
// them and their data copied to the data section.
//
list_packet *send_packet( int command, int from, int to, const char __user *buf, size_t count )
{
	fp_device_t	*to_dev = NULL;
	list_packet	*lptr = NULL, *llptr = NULL;
	read_packet	*rptr   = NULL;
	struct list_head *node = NULL;
	struct list_head *next = NULL;
	size_t          len    = 0;
	size_t	        res    = 0;
	int	        i;

	if( count < 0 ) {
		return( NULL );
	}

	if( from == FPM_DEV ) {
		len = count + (sizeof( list_packet ) - sizeof( read_packet ));	// count is the size of the read_packet only
		if( (lptr = kcalloc( 1, len, GFP_KERNEL )) == NULL ) {         	// allocate a record
			return( NULL );
		}

		rptr = &lptr->packet;						// get a handle to the read_packet within the list_packet
		if( (res = copy_from_user( &lptr->packet, buf, count ) ) < 0 ) {	// copy data from user space
			kfree( lptr );
			return( NULL );
		}
	} else {
		len = sizeof( list_packet ) + count;
		if( (lptr = kcalloc( 1, len, GFP_KERNEL )) == NULL ) {		// allocate a record
			return( NULL );
		}
		rptr = &lptr->packet;						// get a handle to the read_packet within the list_packet
		rptr->command = command;					// the payload is always DATA from here
		rptr->from    = from;						// load the originating device
		rptr->to      = to;						// load the destination device

		if (count > 0) {
			if (access_ok(VERIFY_READ, (void __user *)buf, count)) {
				if( (res = copy_from_user( &rptr->data, buf, count ) ) < 0 ) {
					kfree( lptr );
					return( NULL );
				}
				rptr->size  = count - res;	// copy_from_user returns number of bytes NOT transfered
			} else {
				memcpy(&rptr->data, buf, count);
				rptr->size = count;
			}
		}
	}

	pr_debug("%s: from=%s, to=%s, size=%d, data=%p, total=%d, cmd=%d\n", __func__,
		slot_to_string(lptr->packet.from), slot_to_string(lptr->packet.to),
		lptr->packet.size, lptr->packet.data, len, rptr->command);

	if (rptr->to < FP_MAX_DEVS) {
		to_dev = &fp_devtab[ rptr->to ];

		if( down_interruptible( &to_dev->sem ) ) {
			//return( -ERESTARTSYS );
			return( NULL );
		}
	}

	// separate out signaling commands from the rest and handle them
	switch( rptr->command ) {
		case SIGNAL_ALL:
			pr_debug("%s: SIGNAL_ALL packet\n", __func__);
			for( i = 0; i < FPM_DEV; i++ ) {
				// if the pid is set, send SIGWINCH to it.
				if( (fp_devtab[i].open_flags != CLOSED) && (fp_devtab[i].pid > 1) ) {
					send_signal( fp_devtab[i].pid, SIGWINCH );
				}
			}
			kfree( lptr );
			break;
		case SIGNAL:
			pr_debug("%s: SIGNAL packet to %s\n", __func__, slot_to_string(lptr->packet.to));
			if( (fp_devtab[rptr->to].open_flags != CLOSED) && (fp_devtab[rptr->to].pid > 0) ) {
				send_signal( fp_devtab[rptr->to].pid, SIGWINCH );
			}
			kfree( lptr );
			break;
		case DATA:
			// Check here for AUX switch status packet
			// Update AUX device status
			if (rptr->to == AUX_DEV) {
				pr_debug("%s: AUX status update packet %02x\n", __func__, rptr->data[0]);
				if (rptr->data[0] == AUX_SWITCH_ON)
					fp_devtab[AUX_DEV].aux_state = AUX_SWITCH_ON;
				else if (rptr->data[0] == AUX_SWITCH_OFF)
					fp_devtab[AUX_DEV].aux_state = AUX_SWITCH_OFF;
				// Delete any earlier status packets from queue
				list_for_each_safe( node, next, &to_dev->vroot ) {
					llptr = container_of( node, list_packet, links );
					list_del( &llptr->links );		// unlink this structure from the list
					kfree( llptr ); 				// free this record
				}
			}
			// If raw-only packet, and to_dev is not open direct mode, discard
			if ((rptr->to < APP_OPENS) && (rptr->raw_offset == 0) && !(to_dev->open_flags & DR_OPEN)) {
				kfree(lptr);
				lptr = NULL;
				break;
			}

			list_add_tail( &lptr->links, &to_dev->vroot );				// load onto queue for the FPM
			wake_up_interruptible( &to_dev->wait );					// wake up any blocked processes waiting on this queue
			break;
		case FOCUS:
			pr_debug("Setting Focus to %x %x %x %x\n",
					lptr->packet.data[0], lptr->packet.data[1],
					lptr->packet.data[2], lptr->packet.data[3] );
			list_add_tail( &lptr->links, &to_dev->vroot );				// load onto queue for the FPM
			wake_up_interruptible( &to_dev->wait );					// wake up any blocked processes waiting on this queue
			break;
		default:
			list_add_tail( &lptr->links, &to_dev->vroot );				// load onto queue for the FPM
			wake_up_interruptible( &to_dev->wait );					// wake up any blocked processes waiting on this queue
			break;
	}

	if (to_dev != NULL)
		up( &to_dev->sem );

	return( lptr );
}


#ifdef CONFIG_FP_PROC_FS
/*
 * The proc filesystem: function to read and entry
 */

int fp_read_procmem(char *buf, char **start, off_t offset, int count, int *eof, void *data)
{
	int i   = 0;
	int len = 0;
	list_packet      * lptr   = NULL;
	struct list_head * node   = NULL;
	struct list_head * next   = NULL;

	for (i = 0; i < FP_MAX_DEVS; i++) {
		fp_device_t * fp_dev = &fp_devtab[i];

		if (down_interruptible( &fp_dev->sem )) {
			return -ERESTARTSYS;
		}

		len += sprintf(buf+len, "Slot %s: open_flags=0x%x ", slot_to_string(fp_dev->slot), fp_dev->open_flags );
		len += sprintf(buf+len, "next=%p, prev=%p (read queue is %s)\n",
			fp_dev->vroot.next, fp_dev->vroot.prev,
			(list_empty( &fp_dev->vroot ))?"empty":"not empty");

		// dump any pending read records
		list_for_each_safe( node, next, &fp_dev->vroot ) {
			lptr = container_of( node, list_packet, links );
			len += sprintf(buf+len, "\tsize=%4ld, data=%p\n", (long)lptr->packet.size, lptr->packet.data );
		}

		up( &fp_dev->sem );
	}
	*eof = 1;
	return len;
}
#endif




module_init(fp_init);
module_exit(fp_exit);
