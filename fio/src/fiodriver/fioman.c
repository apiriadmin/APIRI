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

FIOMAN Code Module.

TEG - NO LOCKING IS IN PLACE.  THIS IS NOT AN ISSUE FOR INITIAL DEVELOPMENT
	  BUT MUST BE RESOLVE BEFORE RELEASE.

*/
/*****************************************************************************/

/*  Include section.
-----------------------------------------------------------------------------*/
/* System includes. */
#include	<linux/fs.h>		/* File structure definitions */
#include	<linux/errno.h>		/* Errno Definitions */
#include	<linux/slab.h>		/* Memory Definitions */
#include	<linux/time.h>
#include	<linux/kfifo.h>
#include	<asm/uaccess.h>		/* User Space Access Definitions */
#include        <linux/termios.h>

/* Local includes. */
#include	"fioman.h"			/* FIOMAN Definitions */
#include	"fiomsg.h"
#include	"fioframe.h"

/* Inlined C files */
/*#include	"fiomsg.c"*/		/* FIOMSG Code */
/*#include	"fioframe.c"*/
#ifdef FAULTMON_GPIO
#include <linux/gpio.h>
extern int faultmon_gpio;
#endif

/*  Definition section.
-----------------------------------------------------------------------------*/
FIOMSG_PORT	fio_port_table[ FIO_PORTS_MAX ];
/* List used for managing FIOD / Port combinations */
struct list_head	fioman_fiod_list;

FIO_DEV_HANDLE		fioman_fiod_dev_next;
#ifdef LAZY_CLOSE
struct list_head        priv_list;
#endif
int local_time_offset = 0;

/*  Private API declaration (prototype) section.
-----------------------------------------------------------------------------*/
extern int sdlc_kernel_ioctl(void *context, int command, unsigned long parameters);
int fioman_hm_register( struct file *, unsigned int);
int fioman_hm_deregister( struct file *);
int fioman_hm_heartbeat( struct file *);
struct work_struct      hm_timeout_work;
FIOMAN_PRIV_DATA        *hm_timeout_priv = NULL;


/*  Public API interface section.
-----------------------------------------------------------------------------*/


/*  Private API implementation section.
-----------------------------------------------------------------------------*/


/*****************************************************************************/
/*
Function to find if a frame is scheduled for a fio device (in the tx frame list)
*/
/*****************************************************************************/

bool
fioman_frame_is_scheduled
(
	FIOMAN_SYS_FIOD *p_sys_fiod,
	int frame_no
)
{
	struct list_head	*p_elem;	/* Ptr to queue element being examined */
	struct list_head	*p_next;	/* Temp Ptr to next for loop */
	FIOMSG_TX_FRAME		*p_tx_elem;	/* Ptr to tx frame being examined */
	FIOMSG_PORT		*p_port;	/* Port of Request Queue */

        /* Search for frame in tx queue */
        /* Get port to work on */
        p_port = FIOMSG_P_PORT( p_sys_fiod->fiod.port );
	/* For each element in the queue */
	list_for_each_safe( p_elem, p_next, &p_port->tx_queue ) {
		/* Get the request frame for this queue element */
		p_tx_elem = list_entry( p_elem, FIOMSG_TX_FRAME, elem );
		/* See if current element matches fiod */
		if ( p_tx_elem->fiod.fiod == p_sys_fiod->fiod.fiod ) {
			/* Does the frame number match one requested? */
			if (FIOMSG_PAYLOAD( p_tx_elem )->frame_no == frame_no) {
                                return true;
			}
		}
	}
        return false;
}

/*****************************************************************************/

/*****************************************************************************/
/*
Function to find the FIOD in APP List
*/
/*****************************************************************************/

FIOMAN_APP_FIOD	*
fioman_find_dev
(
	FIOMAN_PRIV_DATA	*p_priv,		/* APP Private Data */
	FIO_DEV_HANDLE		dev_handle		/* Device Handle to Locate */
)
{
	struct list_head	*p_app_elem;	/* Ptr to app element being examined */
	FIOMAN_APP_FIOD		*p_app_fiod;	/* Ptr to app fiod structure */

	/* Find this APP registeration */
	p_app_fiod = NULL;
	list_for_each( p_app_elem, &p_priv->fiod_list )
	{
		/* Get a ptr to this list entry */
		p_app_fiod = list_entry( p_app_elem, FIOMAN_APP_FIOD, elem );

		/* See if this is what we are looking for */
		if ( dev_handle == p_app_fiod->dev_handle )
		{
			/* YES, found the entry */
			break;
		}

		/* For next time */
		p_app_fiod = NULL;
	}

	/* Return what we found */
	return ( p_app_fiod );
}

/*****************************************************************************/

/*****************************************************************************/
/*
Do the actual work of disabling a FIOD.
*/
/*****************************************************************************/

void
fioman_do_disable_fiod
(
	FIOMAN_APP_FIOD	*p_app_fiod	/* Ptr to APP FIOD */
)
{
	FIOMAN_SYS_FIOD		*p_sys_fiod;	/* Ptr to FIOMAN fiod structure */
	int			ii;				/* Loop variable */
	unsigned long		flags;
	
	/* Set app outputs to off state on disable */
	/* For each plus / minus byte */
	p_sys_fiod = p_app_fiod->p_sys_fiod;
	spin_lock_irqsave(&p_sys_fiod->lock, flags);
	for (ii = 0; ii < FIO_OUTPUT_POINTS_BYTES; ii++)
	{
		/* Turn off all bits we are allowed to touch */
                p_sys_fiod->outputs_plus[ii]  &=~(p_app_fiod->outputs_reserved[ii]);
                p_sys_fiod->outputs_minus[ii] &=~(p_app_fiod->outputs_reserved[ii]);
	}
	spin_unlock_irqrestore(&p_sys_fiod->lock, flags);
	 
	/* Show comm being disabled */
	p_app_fiod->enabled = false;

	/* See if this is the last APP to disable */
	if ( 0 == ( --p_app_fiod->p_sys_fiod->app_enb ) )
	{
		/* Last APP disabling FIOD */
		/* TEG - I need to make sure that the set_outputs frame(s) are NOT
		   removed by the remove below.  Also, FIOMAN may have to do this
		   as the APPs need to know that the outputs are now 0 */
		/* Last access to this FIOD on this port */
		/* First set all output points on this FIOD to 0 */
		// TEG - TODO fiomsg_tx_set_outputs_0( p_port, p_arg->fiod );

		/* Second remove frames destinated for FIOD */
		fiomsg_tx_remove_fiod( &p_app_fiod->fiod );

		/* Lastly, remove response frames for FIOD */
		fiomsg_rx_remove_fiod( &p_app_fiod->fiod );
	}

	/* Show an APP disabling the port */
	fiomsg_port_disable( &p_app_fiod->fiod );

	/* If comm has now been disabled */
	if ( 0 == fiomsg_port_comm_status( &p_app_fiod->fiod ) )
	{
		FIO_IOC_FIOD	fiod;	/* Tmp FIOD for clean up */

		/* Set up FIOD for clean up */
		fiod.port = p_app_fiod->fiod.port;
		fiod.fiod = FIOD_ALL;

		/* This is the last comm to this port */
		/* Remove frames listed as FIOD_ALL */
		fiomsg_tx_remove_fiod( &fiod );
	
		/* The queue should be empty at this point, flag it if not */
		if (! list_empty( &FIOMSG_P_PORT( fiod.port )->tx_queue ) )
		{
			printk( KERN_ALERT "TX Queue is not empty!\n" );
		}

		/* No response frames for FIOD_ALL */
		//fiomsg_rx_remove_fiod( &fiod );
	}
}

/*****************************************************************************/

/*****************************************************************************/
/*
Do the actual FIOD deregistration.
*/
/*****************************************************************************/

void
fioman_do_dereg_fiod
(
	FIOMAN_APP_FIOD	*p_app_fiod	/* Ptr to APP FIOD */
)
{
	FIOMAN_SYS_FIOD		*p_sys_fiod;	/* Ptr to FIOMAN fiod structure */
#ifdef NEW_WATCHDOG
	struct list_head	*p_app_elem;	/* Ptr to app element being examined */
        FIO_HZ                  new_rate;
#endif

	/* See if this FIOD is currently enabled */
	if ( p_app_fiod->enabled )
	{
		/* Disable the FIOD, as the APP currently has enabled */
		fioman_do_disable_fiod( p_app_fiod );
	}

	/* Get pointer to FIOMAN list element */
	p_sys_fiod = p_app_fiod->p_sys_fiod;

	/* Clean up the App list */
	list_del_init(&p_app_fiod->elem);
	/* Clean up the Sys list */
	list_del_init(&p_app_fiod->sys_elem);
	/* Clean up the transition app transition fifo */
	FIOMAN_FIFO_FREE(p_app_fiod->transition_fifo);
	
	kfree( p_app_fiod );
	
	/* See if this is the last registration */
	if ( 0 == --p_sys_fiod->app_reg )
	{
		/* YES, last reference.  Get rid of this list element */
		list_del_init( &p_sys_fiod->elem );
		kfree( p_sys_fiod );
	}
#ifdef NEW_WATCHDOG
	 else {
                /* re-assess the watchdog rate if required */
                if ((p_sys_fiod->watchdog_output >= 0)
                        && (p_sys_fiod->watchdog_rate != FIO_HZ_0)) {
                        new_rate = FIO_HZ_0;
                        p_app_fiod = NULL;
                        list_for_each( p_app_elem, &p_sys_fiod->app_fiod_list )
                        {
                                /* Get a ptr to this list entry */
                                p_app_fiod = list_entry( p_app_elem, FIOMAN_APP_FIOD, sys_elem );
                                if (p_app_fiod->watchdog_reservation) {
                                        if (p_app_fiod->watchdog_rate > new_rate) {
                                                new_rate = p_app_fiod->watchdog_rate;
                                        }
                                }
                        }

                        if (new_rate !=  p_sys_fiod->watchdog_rate) {
                                p_sys_fiod->watchdog_rate = new_rate;
                                pr_debug("fioman_do_dereg_fiod: new wd rate %d\n", new_rate);
                        }
                }
        }
#endif
}

/*****************************************************************************/

/*****************************************************************************/
/*
This function is used to add a list of frames for TX.
*/
/*****************************************************************************/

void
fioman_add_frames
(
	FIO_IOC_FIOD		*fiod,		/* FIOD to add to */
	struct list_head	*tx_frames,	/* TX Frames to be added */
	struct list_head	*rx_frames	/* RX Frames to be added */
)
{
	struct list_head	*p_elem;	/* Ptr to queue element being examined */
	struct list_head	*p_next;	/* Temp Ptr to next for loop */
	FIOMSG_TX_FRAME		*p_tx_elem;	/* Ptr to tx frame being examined */
	FIOMSG_RX_FRAME		*p_rx_elem;	/* Ptr to rx frame being examined */

	/* For each element in the list */
	list_for_each_safe( p_elem, p_next, tx_frames )
	{
		/* Get the request frame for this queue element */
		p_tx_elem = list_entry( p_elem, FIOMSG_TX_FRAME, elem );

		/* Move from this list to TX queue */
		fiomsg_tx_add_frame( FIOMSG_P_PORT( fiod->port ), p_tx_elem );
	}

	/* For each element in the list */
	list_for_each_safe( p_elem, p_next, rx_frames )
	{
		/* Get the request frame for this queue element */
		p_rx_elem = list_entry( p_elem, FIOMSG_RX_FRAME, elem );

		/* Move from this list to RX queue */
		fiomsg_rx_add_frame( FIOMSG_P_PORT( fiod->port ), p_rx_elem );
	}
}

/*****************************************************************************/

/*****************************************************************************/
/*
This function is used to free up frames that are waiting to be added to
a TX queue.
*/
/*****************************************************************************/

void
fioman_free_frames
(
	struct list_head	*tx_frames,		/* TX Frames to be freed */
	struct list_head	*rx_frames		/* RX Frames to be freed */
)
{
	struct list_head	*p_elem;	/* Ptr to queue element being examined */
	struct list_head	*p_next;	/* Temp Ptr to next for loop */
	FIOMSG_TX_FRAME		*p_tx_elem;	/* Ptr to tx frame being examined */
	FIOMSG_RX_FRAME		*p_rx_elem;	/* Ptr to rx frame being examined */

	/* For each element in the list */
	list_for_each_safe( p_elem, p_next, tx_frames )
	{
		/* Get the request frame for this queue element */
		p_tx_elem = list_entry( p_elem, FIOMSG_TX_FRAME, elem );

		/* YES, then remove this element */
		list_del_init( p_elem );

		/* Free memory with this TX buffer */
		kfree( p_tx_elem );
	}

	/* For each element in the list */
	list_for_each_safe( p_elem, p_next, rx_frames )
	{
		/* Get the request frame for this queue element */
		p_rx_elem = list_entry( p_elem, FIOMSG_RX_FRAME, elem );

		/* YES, then remove this element */
		list_del_init( p_elem );

		/* Free memory with this RX buffer */
		kfree( p_rx_elem );
	}
}

/*****************************************************************************/

/*****************************************************************************/
/*
This function is used to ready a specific request frame and response, if any
*/
/*****************************************************************************/
int
fioman_add_frame
(
	int frame_no,
	FIOMAN_SYS_FIOD	*p_sys_fiod		/* FIOD of destined frame */
)
{
	FIOMSG_TX_FRAME *txframe = NULL;        /* Ptr to tx frame */
	FIOMSG_RX_FRAME *rxframe = NULL;        /* Ptr to rx frame */

        switch (p_sys_fiod->fiod.fiod) {
        case FIOCMU:
	/* ITS CMU */
	{
                switch (frame_no) {
                case 62:
                {
                        txframe = fioman_ready_frame_62( p_sys_fiod );
                        rxframe = fioman_ready_frame_190( p_sys_fiod );
                        break;
                }
                case 66:
                {
                        txframe = fioman_ready_frame_66( p_sys_fiod );
                        break;
                }
                case 67:
                {
                        txframe = fioman_ready_frame_67( p_sys_fiod );
                        rxframe = fioman_ready_frame_195( p_sys_fiod );
                }
                case 61:
                {
                        txframe = fioman_ready_frame_61( p_sys_fiod );
                        rxframe = fioman_ready_frame_189( p_sys_fiod );
                }
                case 65: default:
                        break;
                }
                break;
        }
        case FIOMMU:
        /* NEMA MMU */
        {
                switch (frame_no) {
                case 0:
                {
                        txframe = fioman_ready_frame_0( p_sys_fiod );
                        rxframe = fioman_ready_frame_128( p_sys_fiod );
                        break;
                }
                case 1:
                {
                        txframe = fioman_ready_frame_1( p_sys_fiod );
                        rxframe = fioman_ready_frame_129( p_sys_fiod );
                        break;
                }
                case 3: default:
                        break;
                }
                break;
        }
        case FIODR1:case FIODR2:case FIODR3:case FIODR4:
        case FIODR5:case FIODR6:case FIODR7:case FIODR8:
        /* NEMA DR BIU */
        {
                if (frame_no >= FIOMAN_FRAME_NO_20 && frame_no <= FIOMAN_FRAME_NO_23) {
                        txframe = fioman_ready_frame_20_23(p_sys_fiod, frame_no);
                        rxframe = fioman_ready_frame_148_151(p_sys_fiod, frame_no + 128);
                } else if (frame_no >= FIOMAN_FRAME_NO_23 && frame_no <= FIOMAN_FRAME_NO_27) {
                        txframe = fioman_ready_frame_24_27(p_sys_fiod, frame_no);
                        rxframe = fioman_ready_frame_152_155(p_sys_fiod, frame_no + 128);
		}
                break;
        }
        case FIOTF1:case FIOTF2:case FIOTF3:case FIOTF4:
        case FIOTF5:case FIOTF6:case FIOTF7:case FIOTF8:
        /* NEMA TF BIU */
        {
                if (frame_no >= FIOMAN_FRAME_NO_10 && frame_no <= FIOMAN_FRAME_NO_11) {
                        txframe = fioman_ready_frame_10_11(p_sys_fiod, frame_no);
                        rxframe = fioman_ready_frame_138_141(p_sys_fiod, frame_no + 128);
                }
                if (frame_no >= FIOMAN_FRAME_NO_12 && frame_no <= FIOMAN_FRAME_NO_13) {
                        txframe = fioman_ready_frame_12_13(p_sys_fiod, frame_no);
                        rxframe = fioman_ready_frame_138_141(p_sys_fiod, frame_no + 128);
                }
                break;
        }
        case FIOINSIU1:case FIOINSIU2:case FIOINSIU3:case FIOINSIU4:case FIOINSIU5:
        case FIOOUT6SIU1:case FIOOUT6SIU2:case FIOOUT6SIU3:case FIOOUT6SIU4:
        case FIOOUT14SIU1:case FIOOUT14SIU2:
        case FIO332:case FIOTS1:case FIOTS2:
        /* ITS SIU/Caltrans 332(2070-2A)/TS1(2070-8)/TS2(2070-2N)*/
        {
                switch (frame_no) {
                case 49:
                {
                        txframe = fioman_ready_frame_49(p_sys_fiod);
                        rxframe = fioman_ready_frame_177(p_sys_fiod);
                        break;
                }
                case 51:
                {
                        txframe = fioman_ready_frame_51(p_sys_fiod);
                        rxframe = fioman_ready_frame_179(p_sys_fiod);
                        break;
                }
                case 52:
                {
                        txframe = fioman_ready_frame_52(p_sys_fiod);
                        rxframe = fioman_ready_frame_180(p_sys_fiod);
                        break;
                }
                case 53:
                {
                        txframe = fioman_ready_frame_53(p_sys_fiod);
                        rxframe = fioman_ready_frame_181(p_sys_fiod);
                        break;
                }
                case 54:
                {
                        txframe = fioman_ready_frame_54(p_sys_fiod);
                        rxframe = fioman_ready_frame_182(p_sys_fiod);
                        break;
                }
                case 55:
                {
                        txframe = fioman_ready_frame_55(p_sys_fiod);
                        rxframe = fioman_ready_frame_183(p_sys_fiod);
                        break;
                }
                case 60: 
                {
                        txframe = fioman_ready_frame_60(p_sys_fiod);
                        rxframe = fioman_ready_frame_188(p_sys_fiod);
                        break;
                }
                default:
                        break;
                }
                break;
        }

        default:
                break;
        }

        if (txframe == NULL)
                return -EINVAL;
        
        if (IS_ERR(txframe))
                return PTR_ERR(txframe);

        if ((rxframe == NULL) || !IS_ERR(rxframe)) {
                /* Add to TX queue */
                txframe->cur_freq = p_sys_fiod->frame_frequency_table[frame_no];
		fiomsg_tx_add_frame(FIOMSG_P_PORT(p_sys_fiod->fiod.port), txframe);
                if (rxframe != NULL)
                        /* Add to RX queue */
                        fiomsg_rx_add_frame(FIOMSG_P_PORT(p_sys_fiod->fiod.port), rxframe);
                return 0;
        }
        
        return PTR_ERR(rxframe);
}

/*****************************************************************************/
/*
This function adds the default frames for a port (given the indicated FIOD)
to the request frame list for this port, for the first time the port is
accessed.
*/
/*****************************************************************************/

int
fioman_add_def_port_frames
(
	FIOMAN_SYS_FIOD		*p_fiod,	/* Port / FIOD Combination */
	struct list_head	*tx_frames,	/* TX frames list to add to */
	struct list_head	*rx_frames	/* RX frames list to add to */
)
{
	/* Enable either frame 66 or frame 9 (or nothing), based upon FIOD */
	/* TEG - FOR NOW, This code only supports Model 332 cabinets, until */
	/* Work on other cabinets commences. */
	/* Therefore, only frame 66 is added by this code */
	/* A table, needs to be added to figure out what frames need to be */
	/* added for this port based upon the FIOD that is being accessed */

	switch ( p_fiod->fiod.fiod )
	{
		case FIOCMU:
		case FIOINSIU1:case FIOINSIU2:case FIOINSIU3:case FIOINSIU4:case FIOINSIU5:
		case FIOOUT6SIU1:case FIOOUT6SIU2:case FIOOUT6SIU3:case FIOOUT6SIU4:
		case FIOOUT14SIU1:case FIOOUT14SIU2:
		/* ITS */
		{
			void	*frame_66;	/* Pointer for alloc'ed frame 66 */

			/* Ready frame 66 for this port */
			frame_66 = fioman_ready_frame_66( p_fiod );

			/* Make sure frame was created */
			/* If not system will fail */
			if ( IS_ERR( frame_66 ) )
			{
				/* Return failure */
				return ( PTR_ERR( frame_66 ) );
			}

			/* Add Date / Time broadcast frame 66 */
			list_add_tail( &((FIOMSG_TX_FRAME *)(frame_66))->elem, tx_frames );

			break;
		}

		case FIOMMU:
		case FIODR1:case FIODR2:case FIODR3:case FIODR4:
		case FIODR5:case FIODR6:case FIODR7:case FIODR8:
		case FIOTF1:case FIOTF2:case FIOTF3:case FIOTF4:
		case FIOTF5:case FIOTF6:case FIOTF7:case FIOTF8:
		/* NEMA */
		{
			void	*tx_frame;	/* Pointer for alloc'ed frame 9 */

			/* Ready frame 9 for this port */
			tx_frame = fioman_ready_frame_9( p_fiod );

			/* Make sure frame was created */
			/* If not system will fail */
			if ( IS_ERR( tx_frame ) )
			{
				/* Return failure */
				return ( PTR_ERR( tx_frame ) );
			}

			/* Add Date / Time broadcast frame 9 */
			list_add_tail( &((FIOMSG_TX_FRAME *)(tx_frame))->elem, tx_frames );

			/* Add outputs transfer request frame 18 */
			tx_frame = fioman_ready_frame_18( p_fiod );
			if ( IS_ERR( tx_frame ) ) {
				/* Return failure */
				return ( PTR_ERR( tx_frame ) );
			}

			if (tx_frame)
				list_add_tail( &((FIOMSG_TX_FRAME *)(tx_frame))->elem, tx_frames );
			break;
		}

		case FIO332:case FIOTS1:case FIOTS2:
		/* Caltrans 332(2070-2A) or TS1(2070-8) or TS2(2070-2N) */
			break;

		default:
		/* TEG - Other cabinets */
		{
			return ( -EINVAL );
		}
	}

	/* Succeeded */
	return ( 0 );
}

/*****************************************************************************/

/*****************************************************************************/
/*
This function adds the default frames for a FIOD the first time
to the request frame list for this port.  The port semaphore is already locked
by the calling function, so it is safe to access the port table.
*/
/*****************************************************************************/

int
fioman_add_def_fiod_frames
(
	FIOMAN_SYS_FIOD		*p_fiod,	/* Port / FIOD Combination */
	struct list_head	*tx_frames,	/* List of TX frames to add */
	struct list_head	*rx_frames	/* List of RX frames to add */
)
{
	void	*tx_frame = NULL;	/* Pointer for alloc'ed frame */
	void	*rx_frame = NULL;	/* Pointer for alloc'ed frame */
/* TEG - TODO */
	/* Enable input frames */
	/* Enable output frames */
	/* Enable CMU frames */
	/* Enable MMU frames */

/* TEG - TODO */
	/* FOR NOW, This code only supports Model 332 cabinets, until */
	/* Work on other cabinets commences. */
	/* Therefore, only Model 332 cabinet frames are added by this code */
	/* A table, needs to be added to figure out what frames need to be */
	/* added for this port based upon the FIOD that is being accessed */

	switch ( p_fiod->fiod.fiod )
	{
		/* ITS CMU */
		case FIOCMU:
		{
			/* send load switch drivers frame 67 + short status response frame 195 */
			/* Ready frame 67 for this port */
			tx_frame = fioman_ready_frame_67( p_fiod );

			/* Make sure frame was created */
			/* If not system will fail */
			if ( IS_ERR( tx_frame ) ) {
				/* Return failure */
				return ( PTR_ERR( tx_frame ) );
			}

			/* Ready corresponding response frame */
			rx_frame = fioman_ready_frame_195( p_fiod );

			/* Make sure frame was created */
			/* If not, user can never access frame */
			if ( IS_ERR( rx_frame ) ) {
				/* Return failure */
				return ( PTR_ERR( rx_frame ) );
			}

			/* Add tx frame to list, if required */
			if (tx_frame)
				list_add_tail( &((FIOMSG_TX_FRAME *)(tx_frame))->elem, tx_frames );

			/* Add response frame to RX list, if required */
			if (rx_frame)
				list_add_tail( &((FIOMSG_RX_FRAME *)(rx_frame))->elem, rx_frames );

#if 1
			/* Indicate other valid frames not sent by default (FIO332,FIOTS1,FIOINSIU,FIOOUTSIU) */
			p_fiod->frame_frequency_table[60] = FIO_HZ_0;
			p_fiod->frame_frequency_table[61] = FIO_HZ_0;
			p_fiod->frame_frequency_table[62] = FIO_HZ_0;
			p_fiod->frame_frequency_table[65] = FIO_HZ_0;
#endif
			break;
		}

		/* NEMA */
		case FIOMMU:
		{
			/* send mmu programming request frame 3 + response frame 131 */
			/* Ready frame 3 for this port */
			tx_frame = fioman_ready_frame_3( p_fiod );

			/* Make sure frame was created */
			/* If not system will fail */
			if ( IS_ERR( tx_frame ) ) {
				/* Return failure */
				return ( PTR_ERR( tx_frame ) );
			}

			/* Ready corresponding response frame */
			rx_frame = fioman_ready_frame_131( p_fiod );

			/* Make sure frame was created */
			/* If not, user can never access frame */
			if ( IS_ERR( rx_frame ) ) {
				/* Return failure */
				return ( PTR_ERR( rx_frame ) );
			}

			/* Add tx frame to list, if required */
			if (tx_frame)
				list_add_tail( &((FIOMSG_TX_FRAME *)(tx_frame))->elem, tx_frames );

			/* Add response frame to RX list, if required */
			if (rx_frame)
				list_add_tail( &((FIOMSG_RX_FRAME *)(rx_frame))->elem, rx_frames );
			/* send load switch drivers frame 0 + response frame 128 */
			/* Ready frame 0 for this port */
			tx_frame = fioman_ready_frame_0( p_fiod );

			/* Make sure frame was created */
			/* If not system will fail */
			if ( IS_ERR( tx_frame ) ) {
				/* Return failure */
				return ( PTR_ERR( tx_frame ) );
			}

			/* Ready corresponding response frame */
			rx_frame = fioman_ready_frame_128( p_fiod );

			/* Make sure frame was created */
			/* If not, user can never access frame */
			if ( IS_ERR( rx_frame ) ) {
				/* Return failure */
				return ( PTR_ERR( rx_frame ) );
			}

			/* Add tx frame to list, if required */
			if (tx_frame)
				list_add_tail( &((FIOMSG_TX_FRAME *)(tx_frame))->elem, tx_frames );

			/* Add response frame to RX list, if required */
			if (rx_frame)
				list_add_tail( &((FIOMSG_RX_FRAME *)(rx_frame))->elem, rx_frames );

			/* send mmu status request frame 1 + response frame 129 */
			/* Ready frame 1 for this port */
			tx_frame = fioman_ready_frame_1( p_fiod );

			/* Make sure frame was created */
			/* If not system will fail */
			if ( IS_ERR( tx_frame ) ) {
				/* Return failure */
				return ( PTR_ERR( tx_frame ) );
			}

			/* Ready corresponding response frame */
			rx_frame = fioman_ready_frame_129( p_fiod );

			/* Make sure frame was created */
			/* If not, user can never access frame */
			if ( IS_ERR( rx_frame ) ) {
				/* Return failure */
				return ( PTR_ERR( rx_frame ) );
			}

			/* Add tx frame to list, if required */
			if (tx_frame)
				list_add_tail( &((FIOMSG_TX_FRAME *)(tx_frame))->elem, tx_frames );

			/* Add response frame to RX list, if required */
			if (rx_frame)
				list_add_tail( &((FIOMSG_RX_FRAME *)(rx_frame))->elem, rx_frames );

			break;
		}
		case FIODR1:
		{
			/* Add Call Data Request Frame 20 */
			tx_frame = fioman_ready_frame_20_23( p_fiod, FIOMAN_FRAME_NO_20 );
			if ( IS_ERR( tx_frame ) ) {
				/* Return failure */
				return ( PTR_ERR( tx_frame ) );
			}
			/* Ready corresponding response frame */
			rx_frame = fioman_ready_frame_148_151( p_fiod, FIOMAN_FRAME_NO_148 );
			if ( IS_ERR( rx_frame ) ) {
				/* Return failure */
				return ( PTR_ERR( rx_frame ) );
			}

			if (tx_frame)
				list_add_tail( &((FIOMSG_TX_FRAME *)(tx_frame))->elem, tx_frames );

			if (rx_frame)
				list_add_tail( &((FIOMSG_RX_FRAME *)(rx_frame))->elem, rx_frames );

			/* Indicate other valid frames not sent by default */
			p_fiod->frame_frequency_table[24] = FIO_HZ_0;
			
			break;
		}
		case FIODR2:
		{
			/* Add Call Data Request Frame 21 */
			tx_frame = fioman_ready_frame_20_23( p_fiod, FIOMAN_FRAME_NO_21 );
			if ( IS_ERR( tx_frame ) ) {
				/* Return failure */
				return ( PTR_ERR( tx_frame ) );
			}
			/* Ready corresponding response frame */
			rx_frame = fioman_ready_frame_148_151( p_fiod, FIOMAN_FRAME_NO_149 );
			if ( IS_ERR( rx_frame ) ) {
				/* Return failure */
				return ( PTR_ERR( rx_frame ) );
			}

			if (tx_frame)
				list_add_tail( &((FIOMSG_TX_FRAME *)(tx_frame))->elem, tx_frames );

			if (rx_frame)
				list_add_tail( &((FIOMSG_RX_FRAME *)(rx_frame))->elem, rx_frames );

			/* Indicate other valid frames not sent by default */
			p_fiod->frame_frequency_table[25] = FIO_HZ_0;

			break;
		}

		case FIODR3:
		{
			/* Add Call Data Request Frame 22 */
			tx_frame = fioman_ready_frame_20_23( p_fiod, FIOMAN_FRAME_NO_22 );
			if ( IS_ERR( tx_frame ) ) {
				/* Return failure */
				return ( PTR_ERR( tx_frame ) );
			}
			/* Ready corresponding response frame */
			rx_frame = fioman_ready_frame_148_151( p_fiod, FIOMAN_FRAME_NO_150 );
			if ( IS_ERR( rx_frame ) ) {
				/* Return failure */
				return ( PTR_ERR( rx_frame ) );
			}

			if (tx_frame)
				list_add_tail( &((FIOMSG_TX_FRAME *)(tx_frame))->elem, tx_frames );

			if (rx_frame)
				list_add_tail( &((FIOMSG_RX_FRAME *)(rx_frame))->elem, rx_frames );

			/* Indicate other valid frames not sent by default */
			p_fiod->frame_frequency_table[26] = FIO_HZ_0;

			break;
		}
		case FIODR4:
		{
			/* Add Call Data Request Frame 23 */
			tx_frame = fioman_ready_frame_20_23( p_fiod, FIOMAN_FRAME_NO_23 );
			if ( IS_ERR( tx_frame ) ) {
				/* Return failure */
				return ( PTR_ERR( tx_frame ) );
			}
			/* Ready corresponding response frame */
			rx_frame = fioman_ready_frame_148_151( p_fiod, FIOMAN_FRAME_NO_151 );
			if ( IS_ERR( rx_frame ) ) {
				/* Return failure */
				return ( PTR_ERR( rx_frame ) );
			}

			if (tx_frame)
				list_add_tail( &((FIOMSG_TX_FRAME *)(tx_frame))->elem, tx_frames );

			if (rx_frame)
				list_add_tail( &((FIOMSG_RX_FRAME *)(rx_frame))->elem, rx_frames );

			/* Indicate other valid frames not sent by default */
			p_fiod->frame_frequency_table[27] = FIO_HZ_0;

			break;
		}
		case FIODR5:case FIODR6:case FIODR7:case FIODR8: break;
		case FIOTF1:
		{
			/* send outputs/inputs request frame 10 + response frame 138 */
			tx_frame = fioman_ready_frame_10_11( p_fiod, FIOMAN_FRAME_NO_10 );
			if ( IS_ERR( tx_frame ) ) {
				/* Return failure */
				return ( PTR_ERR( tx_frame ) );
			}
			/* Ready corresponding response frame */
			rx_frame = fioman_ready_frame_138_141( p_fiod, FIOMAN_FRAME_NO_138 );
			if ( IS_ERR( rx_frame ) ) {
				/* Return failure */
				return ( PTR_ERR( rx_frame ) );
			}

			if (tx_frame)
				list_add_tail( &((FIOMSG_TX_FRAME *)(tx_frame))->elem, tx_frames );

			if (rx_frame)
				list_add_tail( &((FIOMSG_RX_FRAME *)(rx_frame))->elem, rx_frames );

			break;
		}
		case FIOTF2:
		{
			/* send outputs/inputs request frame 11 + response frame 139 */
			tx_frame = fioman_ready_frame_10_11( p_fiod, FIOMAN_FRAME_NO_11 );
			if ( IS_ERR( tx_frame ) ) {
				/* Return failure */
				return ( PTR_ERR( tx_frame ) );
			}
			/* Ready corresponding response frame */
			rx_frame = fioman_ready_frame_138_141( p_fiod, FIOMAN_FRAME_NO_139 );
			if ( IS_ERR( rx_frame ) ) {
				/* Return failure */
				return ( PTR_ERR( rx_frame ) );
			}

			if (tx_frame)
				list_add_tail( &((FIOMSG_TX_FRAME *)(tx_frame))->elem, tx_frames );

			if (rx_frame)
				list_add_tail( &((FIOMSG_RX_FRAME *)(rx_frame))->elem, rx_frames );

			break;
		}
		case FIOTF3:
		{
			/* send outputs/inputs request frame 12 + response frame 140 */
			tx_frame = fioman_ready_frame_12_13( p_fiod, FIOMAN_FRAME_NO_12 );
			if ( IS_ERR( tx_frame ) ) {
				/* Return failure */
				return ( PTR_ERR( tx_frame ) );
			}
			/* Ready corresponding response frame */
			rx_frame = fioman_ready_frame_138_141( p_fiod, FIOMAN_FRAME_NO_140 );
			if ( IS_ERR( rx_frame ) ) {
				/* Return failure */
				return ( PTR_ERR( rx_frame ) );
			}

			if (tx_frame)
				list_add_tail( &((FIOMSG_TX_FRAME *)(tx_frame))->elem, tx_frames );

			if (rx_frame)
				list_add_tail( &((FIOMSG_RX_FRAME *)(rx_frame))->elem, rx_frames );

			break;
		}
		case FIOTF4:
		{
			/* send outputs/inputs request frame 13 + response frame 141 */
			tx_frame = fioman_ready_frame_12_13( p_fiod, FIOMAN_FRAME_NO_13 );
			if ( IS_ERR( tx_frame ) ) {
				/* Return failure */
				return ( PTR_ERR( tx_frame ) );
			}
			/* Ready corresponding response frame */
			rx_frame = fioman_ready_frame_138_141( p_fiod, FIOMAN_FRAME_NO_141 );
			if ( IS_ERR( rx_frame ) ) {
				/* Return failure */
				return ( PTR_ERR( rx_frame ) );
			}

			if (tx_frame)
				list_add_tail( &((FIOMSG_TX_FRAME *)(tx_frame))->elem, tx_frames );

			if (rx_frame)
				list_add_tail( &((FIOMSG_RX_FRAME *)(rx_frame))->elem, rx_frames );

			break;
		}
		case FIOTF5:case FIOTF6:case FIOTF7:case FIOTF8: break;

		/* ITS SIU/Caltrans 332(2070-2A)/TS1(2070-8)/TS2(2070-2N)*/
		case FIOINSIU1:case FIOINSIU2:case FIOINSIU3:case FIOINSIU4:case FIOINSIU5:
		case FIOOUT6SIU1:case FIOOUT6SIU2:case FIOOUT6SIU3:case FIOOUT6SIU4:
		case FIOOUT14SIU1:case FIOOUT14SIU2:
		case FIO332:case FIOTS1:case FIOTS2:
		{
#ifdef FAULTMON_GPIO
                        if (faultmon_gpio != -1) {
                                if (p_fiod->fiod.fiod == FIOTS2) {
                                        
                                        break;
                                }
			}
#endif
			/* Ready frame 49 for this device */
			tx_frame = fioman_ready_frame_49( p_fiod );

			/* Make sure frame was created */
			/* If not system will fail */
			if ( IS_ERR( tx_frame ) ) {
				/* Return failure */
				return ( PTR_ERR( tx_frame ) );
			}

			/* Ready corresponding response frame */
			rx_frame = fioman_ready_frame_177( p_fiod );

			/* Make sure frame was created */
			/* If not, user can never access frame */
			if ( IS_ERR( rx_frame ) ) {
				/* Return failure */
				return ( PTR_ERR( rx_frame ) );
			}

			/* Add tx frame to list, if required */
			if (tx_frame)
				list_add_tail( &((FIOMSG_TX_FRAME *)(tx_frame))->elem, tx_frames );

			/* Add response frame to RX list, if required */
			if (rx_frame)
				list_add_tail( &((FIOMSG_RX_FRAME *)(rx_frame))->elem, rx_frames );

			/* Ready frame 55 for this device */
			tx_frame = fioman_ready_frame_55( p_fiod );

			/* Make sure frame was created */
			/* If not system will fail */
			if ( IS_ERR( tx_frame ) ) {
				/* Return failure */
				return ( PTR_ERR( tx_frame ) );
			}

			/* Ready corresponding response frame */
			rx_frame = fioman_ready_frame_183( p_fiod );

			/* Make sure frame was created */
			/* If not, user can never access frame */
			if ( IS_ERR( rx_frame ) ) {
				/* Return failure */
				return ( PTR_ERR( rx_frame ) );
			}

			/* Add tx frame to list, if required */
			if (tx_frame)
				list_add_tail( &((FIOMSG_TX_FRAME *)(tx_frame))->elem, tx_frames );

			/* Add response frame to RX list, if required */
			if (rx_frame)
				list_add_tail( &((FIOMSG_RX_FRAME *)(rx_frame))->elem, rx_frames );

			/* Don't poll inputs for 2070-2N */
			if (p_fiod->fiod.fiod == FIOTS2) {

				break;
			}
			
			/* Ready frame 53 for this device */
			tx_frame = fioman_ready_frame_53( p_fiod );

			/* Make sure frame was created */
			/* If not system will fail */
			if ( IS_ERR( tx_frame ) ) {
				/* Return failure */
				return ( PTR_ERR( tx_frame ) );
			}

			/* Ready corresponding response frame */
			rx_frame = fioman_ready_frame_181( p_fiod );

			/* Make sure frame was created */
			/* If not, user can never access frame */
			if ( IS_ERR( rx_frame ) ) {
				/* Return failure */
				return ( PTR_ERR( rx_frame ) );
			}

			/* Add tx frame to list, if required */
			if (tx_frame)
				list_add_tail( &((FIOMSG_TX_FRAME *)(tx_frame))->elem, tx_frames );

			/* Add response frame to RX list, if required */
			if (rx_frame)
				list_add_tail( &((FIOMSG_RX_FRAME *)(rx_frame))->elem, rx_frames );

#if 1
			/* Indicate other valid frames not sent by default (FIO332,FIOTS1,FIOINSIU,FIOOUTSIU) */
			p_fiod->frame_frequency_table[51] = FIO_HZ_0;
			p_fiod->frame_frequency_table[52] = FIO_HZ_0;
			p_fiod->frame_frequency_table[54] = FIO_HZ_0;
			p_fiod->frame_frequency_table[60] = FIO_HZ_0;
#endif

			break;
		}

		default:
		/* TEG - Other cabinets */
		{
			return ( -EINVAL );
		}
	}

	return ( 0 );
}

/*****************************************************************************/

bool fiod_conflict_check( FIO_DEVICE_TYPE a, FIO_DEVICE_TYPE b)
{
	if ((a > FIO332) && (a < FIOCMU)) {
		if ((b < FIOTS1) || (b > FIOTF8))
			return true;
	} else if (a > FIOTF8) {
		if (b < FIOCMU)
			return true;
	} else if (b != FIO332) {
		return true;
	}
	
	return false;
}

/*****************************************************************************/
/*
Function to allow registration of a FIOD on a port.
*/
/*****************************************************************************/

FIO_DEV_HANDLE
fioman_reg_fiod
(
	struct file		*filp,	/* File Pointer */
	FIO_IOC_FIOD		*fiod	/* FIOD / Port Info */
)
{
	FIOMAN_PRIV_DATA	*p_priv;		/* Access Apps data */

	struct list_head	*p_app_elem;	/* Ptr to app element being examined */
	FIOMAN_APP_FIOD		*p_app_fiod;	/* Ptr to app fiod structure */

	struct list_head	*p_sys_elem;	/* Ptr to FIOMAN elem being examined */
	FIOMAN_SYS_FIOD		*p_sys_fiod;	/* Ptr to FIOMAN fiod structure */
	struct list_head	tx_frames;		/* List of TX frames to add */
	struct list_head	rx_frames;		/* List of RX frames to add */
	int			result;			/* Result of FIOMSG enable */
	bool			sys_present;	/* Is FIOMAN Combo present */
	int i = 0;

	/* Set up our private data */
	p_priv = (FIOMAN_PRIV_DATA *)filp->private_data;

	/* See if this app has already registered this FIOD on port */
	list_for_each( p_app_elem, &p_priv->fiod_list )
	{
		/* Get a ptr to this list entry */
		p_app_fiod = list_entry( p_app_elem, FIOMAN_APP_FIOD, elem );

		/* See if this is what we are looking for */
		if (   ( fiod->fiod == p_app_fiod->fiod.fiod )
			&& ( fiod->port == p_app_fiod->fiod.port ) )
		{
			/* YES, already exists -- return dev_handle */
			return ( p_app_fiod->dev_handle );
		}
	}

	/* See if this FIOD / Port already registered by another App */
	/* See if registering this FIOD / Port would cause a conflict */
	p_sys_fiod = NULL;		/* Show not seen yet */
	list_for_each( p_sys_elem, &fioman_fiod_list )
	{
		/* Get a ptr to this list entry */
		p_sys_fiod = list_entry( p_sys_elem, FIOMAN_SYS_FIOD, elem );

		/* Check if fiod on same port */
		if (fiod->port == p_sys_fiod->fiod.port) {
			if (fiod->fiod == p_sys_fiod->fiod.fiod) {
				/* already exists -- don't re-add */
				break;
			}
			/* Check for conflicts */
			if (fiod_conflict_check(p_sys_fiod->fiod.fiod, fiod->fiod) == true)
				return (FIO_DEV_HANDLE)(-ECONNREFUSED);
		}

		/* Ready for next loop */
		p_sys_fiod = NULL;		/* Show not seen yet */
	}

	/* Malloc all needed memory */
	if ( ! ( p_app_fiod = (FIOMAN_APP_FIOD *)kzalloc( sizeof( FIOMAN_APP_FIOD ), GFP_KERNEL ) ) )
	{
		/* Could not malloc structure */
		return ( (FIO_DEV_HANDLE)( -ENOMEM ) );
	}

	/* See if we found this FIOMAN FIOD / Port Combination, in global list */
	if ( NULL == p_sys_fiod )
	{
		if ( ! ( p_sys_fiod = (FIOMAN_SYS_FIOD *)kzalloc( sizeof( FIOMAN_SYS_FIOD ),
									GFP_KERNEL ) ) )
		{
			/* Could not malloc structure */
			/* free app structure */
			kfree( p_app_fiod );
			return ( (FIO_DEV_HANDLE)( -ENOMEM ) );
		}

		sys_present = false;
	}
	else
	{
		sys_present = true;
	}

	/* Add this FIOD / Port to FIOMAN List, if not already present */
	if ( ! sys_present )
	{
		/* New FIOD / Port combination for FIOMAN */
		INIT_LIST_HEAD( &p_sys_fiod->elem );
		INIT_LIST_HEAD( &p_sys_fiod->app_fiod_list );
		p_sys_fiod->fiod = *fiod;
		spin_lock_init(&p_sys_fiod->lock);
		p_sys_fiod->app_reg = 1;
		p_sys_fiod->status_reset = 0xff;
		p_sys_fiod->dev_handle = fioman_fiod_dev_next++;
		p_sys_fiod->fm_state = FIO_TS_FM_ON;
		p_sys_fiod->vm_state = FIO_TS1_VM_ON;
		p_sys_fiod->cmu_fsa = FIO_CMU_FSA_NONE;
		p_sys_fiod->cmu_mask = FIO_CMU_DC_MASK1;
		p_sys_fiod->cmu_config_change_count = 0;
		p_sys_fiod->watchdog_output = -1;
		p_sys_fiod->watchdog_state = false;
#ifdef NEW_WATCHDOG
                p_sys_fiod->watchdog_rate = FIO_HZ_0;
#else
		p_sys_fiod->watchdog_trigger_condition = false;
#endif
		for (i=0; i<128; i++) {
			/* Indicate invalid frame by default */
			p_sys_fiod->frame_frequency_table[i] = -1;
		}

#if 1
	/* Prepare lists of frames to add */
	INIT_LIST_HEAD( &tx_frames );
	INIT_LIST_HEAD( &rx_frames );

		/* We must add port specific TX frames first */
		if ( 0 > ( result = fioman_add_def_port_frames( p_sys_fiod, &tx_frames, &rx_frames ) ) )
		{
			/* Clean up lists that may have been built */
			fioman_free_frames( &tx_frames, &rx_frames );

			/* Error adding frames */
			return ( result );
		}

		/* Add request frames for this FIOD on this PORT */
		if ( 0 > ( result = fioman_add_def_fiod_frames( p_sys_fiod, &tx_frames, &rx_frames ) ) )
		{
			/* Clean up list that may have been built */
			fioman_free_frames( &tx_frames, &rx_frames );

			/* Error adding frame */
			return ( result );
		}

	/* Add built list to TX & RX queues */
	fioman_add_frames( &p_sys_fiod->fiod, &tx_frames, &rx_frames );

#endif //JMG add frame lists at first reg

                for(i=0; i<(FIO_INPUT_POINTS_BYTES * 8); i++) {
                        p_sys_fiod->input_filters_leading[i] = FIO_FILTER_DEFAULT;
                        p_sys_fiod->input_filters_trailing[i] = FIO_FILTER_DEFAULT;
                }
		list_add_tail( &p_sys_fiod->elem, &fioman_fiod_list );
	}
	else
	{
		/* Show this FIOD / Port combination is registered by another App */
		p_sys_fiod->app_reg++;
	}

	/* Add this FIOD / Port to this Apps list */
	INIT_LIST_HEAD( &p_app_fiod->elem );
	INIT_LIST_HEAD( &p_app_fiod->sys_elem );
	p_app_fiod->fiod = *fiod;
	p_app_fiod->dev_handle = p_sys_fiod->dev_handle;
	for (i=0; i<128; i++) {
		p_app_fiod->frame_frequency_table[i] = p_sys_fiod->frame_frequency_table[i];
	}
	p_app_fiod->fm_state = p_sys_fiod->fm_state;
	p_app_fiod->vm_state = p_sys_fiod->vm_state;
	p_app_fiod->cmu_fsa = p_sys_fiod->cmu_fsa;
	p_app_fiod->cmu_mask = p_sys_fiod->cmu_mask;
	p_app_fiod->watchdog_reservation = false;
	p_app_fiod->watchdog_toggle_pending = false;
#ifdef NEW_WATCHDOG
        p_app_fiod->watchdog_rate = FIO_HZ_0;
#endif
	p_app_fiod->hm_disabled = false;
	p_app_fiod->enabled = false;
        FIOMAN_FIFO_ALLOC(p_app_fiod->transition_fifo, sizeof(FIO_TRANS_BUFFER)*1024, GFP_KERNEL);
        p_app_fiod->transition_status = FIO_TRANS_SUCCESS;
	p_app_fiod->p_sys_fiod = p_sys_fiod;
	list_add_tail( &p_app_fiod->elem, &p_priv->fiod_list );
	/* Add this app fiod to sys fiod list */
	list_add_tail( &p_app_fiod->sys_elem, &p_sys_fiod->app_fiod_list );

	/* Show success */
	return ( p_app_fiod->dev_handle );
}

/*****************************************************************************/

/*****************************************************************************/
/*
Function to allow deregistration of a FIOD on a port.
*/
/*****************************************************************************/

int
fioman_dereg_fiod
(
	struct file		*filp,		/* File Pointer */
	FIO_DEV_HANDLE	dev_handle	/* Previously returned dev_handle */
)
{
	FIOMAN_PRIV_DATA	*p_priv = filp->private_data;	/* Access Apps data */
	FIOMAN_APP_FIOD		*p_app_fiod;	/* Ptr to app fiod structure */

	/* See if this app has already registered this FIOD on port */
	p_app_fiod = fioman_find_dev( p_priv, dev_handle );

	/* See if we found the dev_handle */
	if ( NULL == p_app_fiod )
	{
		/* No, return error */
		return ( -EINVAL );
	}

	/* Do the actual deregistration */
	fioman_do_dereg_fiod( p_app_fiod );

	/* Show success */
	return ( 0 );
}

/*****************************************************************************/


/*****************************************************************************/
/*
Function to enable FIOD.
*/
/*****************************************************************************/

int
fioman_enable_fiod
(
	struct file	*filp,		/* File Pointer */
	FIO_DEV_HANDLE	dev_handle	/* Previously returned dev_handle */
)
{
	FIOMAN_PRIV_DATA	*p_priv = filp->private_data;	/* Access Apps data */
	FIOMAN_APP_FIOD		*p_app_fiod;	/* Ptr to app fiod structure */
	int			result;			/* Result of FIOMSG enable */

	/* Find this APP registration */
	p_app_fiod = fioman_find_dev( p_priv, dev_handle );

	/* See if we found the dev_handle */
	/* Also check if hm fault exists for this device */
	if ( (NULL == p_app_fiod) || p_app_fiod->hm_disabled )
	{
		/* No, return error */
		return ( -EINVAL );
	}


	/* See if comm already enabled by this App to this FIOD */
	if ( p_app_fiod->enabled )
	{
		/* Yes, Do not enable again
		   This test is here so that when disabling comm, comm is
		   truly stopped when this is the last APP to stop comm to
		   FIOD on port.  Otherwise, the reference count of APPs
		   could be incorrect if this APP enabled twice, but only
		   disabled once. */
		return ( 0 );
	}

	/* See if comm is already enabled to port */
	if ( 0 > ( result = fiomsg_port_comm_status( &p_app_fiod->fiod ) ) )
	{
		/* Error with this port */
		return ( result );
	}
#if 0
	/* Prepare lists of frames to add */
	INIT_LIST_HEAD( &tx_frames );
	INIT_LIST_HEAD( &rx_frames );

	/* See if any comm going on yet */
	if ( 0 == result )
	{
		/* NO, This would be the first comm */
		/* Make sure we can enable the serial port for comm */
		if ((result = fiomsg_port_enable(FIOMSG_P_PORT( p_app_fiod->fiod.port))) != 0) {
			printk("fioman_enable_fiod: fiomsg_port_enable=%d\n", result);
			return result;
		}
			
		/* We must add port specific TX frames first */
		if ( 0 > ( result = fioman_add_def_port_frames( p_app_fiod->p_sys_fiod,	&tx_frames, &rx_frames ) ) )
		{
			/* Clean up lists that may have been built */
			fioman_free_frames( &tx_frames, &rx_frames );

			/* Error adding frames */
			return ( result );
		}
	}

	/* See if any APP has already enabled this FIOD */
	if ( 0 == p_app_fiod->p_sys_fiod->app_enb )
	{
		/* Not yet enabled */
		/* Add request frames for this FIOD on this PORT */
		if ( 0 > ( result = fioman_add_def_fiod_frames( p_app_fiod->p_sys_fiod, &tx_frames, &rx_frames ) ) )
		{
			/* Clean up list that may have been built */
			fioman_free_frames( &tx_frames, &rx_frames );

			/* Error adding frame */
			return ( result );
		}

	}

	/* Show enabled by an APP */
	p_app_fiod->p_sys_fiod->app_enb++;

	/* Show this APP enabling comm */
	p_app_fiod->enabled = true;

	/* Add built list to TX & RX queues */
	fioman_add_frames( &p_app_fiod->fiod, &tx_frames, &rx_frames );

#else
	/* See if any comm going on yet */
	if ( 0 == result )
	{
		/* NO, This would be the first comm */
		/* Make sure we can enable the serial port for comm */
		if ((result = fiomsg_port_enable(FIOMSG_P_PORT( p_app_fiod->fiod.port))) != 0) {
			printk("fioman_enable_fiod: fiomsg_port_enable=%d\n", result);
			return result;
		}
        }

	/* Show enabled by an APP */
	p_app_fiod->p_sys_fiod->app_enb++;

	/* Show this APP enabling comm */
	p_app_fiod->enabled = true;

#endif //JMG test add frame lists at first reg.

	/* Start FIOMSG Comm */
	result = fiomsg_port_start( FIOMSG_P_PORT( p_app_fiod->fiod.port ) );

	/* Show result */
	return ( result );
}

/*****************************************************************************/

/*****************************************************************************/
/*
Function to disable FIOD.
*/
/*****************************************************************************/

int
fioman_disable_fiod
(
	struct file		*filp,		/* File Pointer */
	FIO_DEV_HANDLE	dev_handle	/* Previously returned dev_handle */
)
{
	FIOMAN_PRIV_DATA	*p_priv = filp->private_data;	/* Access Apps data */
	FIOMAN_APP_FIOD		*p_app_fiod;	/* Ptr to app fiod structure */

	/* Find this APP registeration */
	p_app_fiod = fioman_find_dev( p_priv, dev_handle );

	/* See if we found the dev_handle */
	if ( NULL == p_app_fiod )
	{
		/* No, return error */
		return ( -EINVAL );
	}

	/* Make sure this APP has already enabled comm */
	if ( ! p_app_fiod->enabled )
	{
		/* Comm not enabled, return error */
		return ( -EINVAL );
	}

	/* Do the disabling */
	fioman_do_disable_fiod( p_app_fiod );

	/* Show success */
	return ( 0 );
}

/*****************************************************************************/

/*****************************************************************************/
/*
Function to query for FIOD on port.
*/
/*****************************************************************************/

int
fioman_query_fiod
(
	struct file		*filp,	/* File Pointer */
	FIO_IOC_QUERY_FIOD	*p_arg	/* Arguments to process */
)
{
	FIOMSG_PORT				*p_port;
	struct list_head		*p_elem;		/* Ptr to list element */
	struct list_head		*p_next;		/* Temp for safe loop */
	FIOMSG_RX_FRAME			*p_rx_frame;	/* Ptr to rx frame being examined */

	if (p_arg->port >= FIO_PORTS_MAX)
		return -EINVAL;
		
	p_port = FIOMSG_P_PORT( p_arg->port );
	if (!p_port->port_opened) {
		/* We must add the device and open port comm */
		/* TBD: Add device if compatible; enable comm; */
	}
	
	/* search rx frames on this port for this fiod */
	list_for_each_safe( p_elem, p_next, &p_port->rx_fiod_list[ p_arg->fiod ] )
	{
		/* Get the response frame for this queue element */
		p_rx_frame = list_entry( p_elem, FIOMSG_RX_FRAME, elem );
		if (p_rx_frame->info.success_rx)
			/* We have successfully received a frame for this device */
			return 1;
	}

	/* We did not find rx frames for this device */
	/* TBD: disable fiod if we added it for this query */
	return 0;
}

/*****************************************************************************/

/*****************************************************************************/
/*
This function is used to reserve output points for an application.
*/
/*****************************************************************************/

int
fioman_reserve_set
(
	struct file			*filp,	/* File Pointer */
	FIO_IOC_RESERVE_SET	*p_arg	/* Arguments to process */
)
{
	FIOMAN_PRIV_DATA	*p_priv = filp->private_data;	/* Access Apps data */
	FIOMAN_APP_FIOD		*p_app_fiod;	/* Ptr to app fiod structure */
	FIOMAN_SYS_FIOD		*p_sys_fiod;	/* Ptr to FIOMAN fiod structure */
	FIOMAN_SYS_FIOD		*p_sys_ref_fiod; /* Ptr to referenced sys fiod */
	struct list_head	*p_sys_elem;	/* Element from FIOMAN FIOD list */
	struct list_head	*p_next;        /* Temp for loop */
	u8			reserve[ FIO_OUTPUT_POINTS_BYTES ] = {0};
	u8			relinquish[ FIO_OUTPUT_POINTS_BYTES ] = {0};
	int			ii, jj;         /* Loop variables */
	unsigned long		flags;

	/* Find this APP registeration */
	p_app_fiod = fioman_find_dev( p_priv, p_arg->dev_handle );

	/* See if we found the dev_handle */
	if ( (NULL == p_app_fiod) || (p_arg->count > FIO_OUTPUT_POINTS_BYTES) )
	{
		/* No, return error */
		return ( -EINVAL );
	}

	/* Copy APP bit array to Kernel Space */
	if ( copy_from_user( reserve, p_arg->data, p_arg->count ) )
	{
		/* failure */
		return ( -EFAULT );
	}

	p_sys_fiod = p_app_fiod->p_sys_fiod;
	/* Check for system-reserved outputs */
	if (((p_sys_fiod->watchdog_output != -1) && FIO_BIT_TEST(reserve, p_sys_fiod->watchdog_output))
		|| ((p_sys_fiod->fiod.fiod == FIOTS1 || p_sys_fiod->fiod.fiod == FIOTS2) && FIO_BIT_TEST(reserve, 78))
		|| ((p_sys_fiod->fiod.fiod == FIOTS1) && FIO_BIT_TEST(reserve, 79)))
		return -ENOTTY;
		
	/* For each reservation byte */
	for ( ii = 0; ii < sizeof( reserve ); ii++ )
	{
		/* Save the complement for reliquishing output points */
		relinquish[ ii ] = ~( reserve[ ii ] );

		/* Isolate differences from what this APP has already reserved */
		reserve[ ii ] &= ~( p_app_fiod->outputs_reserved[ ii ] );

		/* See if these diffs are ok with the FIOMAN list */
		if ( reserve[ ii ] & p_sys_fiod->outputs_reserved[ ii ] )
		{
			/* If not, return error */
			return -ENOTTY;
		}
	}

	/* Set the masks accordingly */
	spin_lock_irqsave(&p_sys_fiod->lock, flags);
	for ( ii = 0; ii < sizeof( reserve ); ii++ )
	{
		/* OR on bits in FIOMAN list */
		/* OR on bits in APP list */
		p_app_fiod->outputs_reserved[ ii ] |= reserve[ ii ];
		p_sys_fiod->outputs_reserved[ ii ] |= reserve[ ii ];

		/* Isolate differences from what this APP has already reserved */
		relinquish[ ii ] &= p_app_fiod->outputs_reserved[ ii ];

		/* Turn off bits in APP and FIOMAN lists now */
		p_app_fiod->outputs_reserved[ ii ] &= ~( relinquish[ ii ] );
		p_sys_fiod->outputs_reserved[ ii ] &= ~( relinquish[ ii ] );
		
		/* Ensure relinquished outputs are reset */
		p_app_fiod->outputs_plus[ ii ]  &= ~( relinquish[ ii ] );
		p_app_fiod->outputs_minus[ ii ] &= ~( relinquish[ ii ] );
		p_sys_fiod->outputs_plus[ ii ]  &= ~( relinquish[ ii ] );
		p_sys_fiod->outputs_minus[ ii ] &= ~( relinquish[ ii ] );

	}
	spin_unlock_irqrestore(&p_sys_fiod->lock, flags);

	/* Also clear any channel mappings associated with relinquished outputs */
	for (ii = 0; ii < (FIO_OUTPUT_POINTS_BYTES*8); ii++) {
		if (FIO_BIT_TEST(relinquish,ii)) {
			/* Search each sys_fiod for mappings for this output point */
			list_for_each_safe (p_sys_elem, p_next, &fioman_fiod_list) {
				/* Get a ptr to this list entry */
				p_sys_ref_fiod = list_entry( p_sys_elem, FIOMAN_SYS_FIOD, elem );
				for (jj = 0; jj < FIO_CHANNELS; jj++) {
					if (p_sys_ref_fiod->channel_map_green[jj] == (ii+1))
						p_sys_ref_fiod->channel_map_green[jj] = 0;
					else if (p_sys_ref_fiod->channel_map_yellow[jj] == (ii+1))
						p_sys_ref_fiod->channel_map_yellow[jj] = 0;
					else if (p_sys_ref_fiod->channel_map_red[jj] == (ii+1))
						p_sys_ref_fiod->channel_map_red[jj] = 0;
				}
			}
		}
	}
	/* return success */
	return ( 0 );
}

/*****************************************************************************/

/*****************************************************************************/
/*
This function is used to get reserved output points for an application.
*/
/*****************************************************************************/

int
fioman_reserve_get
(
	struct file			*filp,	/* File Pointer */
	FIO_IOC_RESERVE_GET	*p_arg	/* Arguments to process */
)
{
	FIOMAN_PRIV_DATA	*p_priv = filp->private_data;	/* Access Apps data */
	FIOMAN_APP_FIOD		*p_app_fiod;	/* Ptr to app fiod structure */
	void				*p_reserve;		/* Reservations to copy */

	/* Find this APP registeration */
	p_app_fiod = fioman_find_dev( p_priv, p_arg->dev_handle );

	/* See if we found the dev_handle */
	if ( (NULL == p_app_fiod) || (p_arg->count > FIO_OUTPUT_POINTS_BYTES) )
	{
		/* No, return error */
		return ( -EINVAL );
	}

	switch ( p_arg->view )
	{
		case FIO_VIEW_APP:	/* Return APP reservations */
		{
			p_reserve = p_app_fiod->outputs_reserved;
			break;
		}

		case FIO_VIEW_SYSTEM: /* Return FIOMAN reservations */
		{
			p_reserve = p_app_fiod->p_sys_fiod->outputs_reserved;
			break;
		}

		default:
		{
			return ( -EINVAL );
		}
	}

	/* Copy Kernel bit array to User Space */
	if ( copy_to_user( p_arg->data, p_reserve, p_arg->count ) )
	{
		return ( -EFAULT );
	}

	return ( 0 );
}

/*****************************************************************************/

/*****************************************************************************/
/*
This function is used to set output points for an application.
*/
/*****************************************************************************/

int
fioman_outputs_set
(
	struct file		*filp,	/* File Pointer */
	FIO_IOC_OUTPUTS_SET	*p_arg	/* Arguments to process */
)
{
	FIOMAN_PRIV_DATA	*p_priv = filp->private_data;	/* Access Apps data */
	FIOMAN_APP_FIOD		*p_app_fiod;	/* Ptr to app fiod structure */
	FIOMAN_SYS_FIOD		*p_sys_fiod;	/* Ptr to FIOMAN fiod structure */
										/* Kernel buffer to use */
	u8					plus[ FIO_OUTPUT_POINTS_BYTES ];
										/* Kernel buffer to use */
	u8					minus[ FIO_OUTPUT_POINTS_BYTES ];
	int					ii;				/* Loop variable */
	unsigned long		flags;

	/* Find this APP registration */
	p_app_fiod = fioman_find_dev( p_priv, p_arg->dev_handle );

	/* See if we found the dev_handle */
	if ( NULL == p_app_fiod )
	{
		/* No, return error */
		return ( -EINVAL );
	}

	/* Copy APP data array to Kernel Space */
	if ( copy_from_user( plus, p_arg->ls_plus, sizeof( plus ) ) )
	{
		/* Could not copy for some reason */
		return ( -EFAULT );
	}

	/* Copy APP ctrl array to Kernel Space */
	if ( copy_from_user( minus, p_arg->ls_minus, sizeof( minus ) ) )
	{
		/* Could not copy for some reason */
		return ( -EFAULT );
	}

	/* For each plus / minus byte */
	p_sys_fiod = p_app_fiod->p_sys_fiod;
	spin_lock_irqsave(&p_sys_fiod->lock, flags);
	for ( ii = 0; ii < sizeof( plus ); ii++ )
	{
		/* Turn off all bits we are allowed to touch */
		p_app_fiod->outputs_plus[ ii ]  &=~(p_app_fiod->outputs_reserved[ ii ]);
		p_app_fiod->outputs_minus[ ii ] &=~(p_app_fiod->outputs_reserved[ ii ]);
                if (!p_priv->transaction_in_progress) {
                        p_sys_fiod->outputs_plus[ ii ]  &=~(p_app_fiod->outputs_reserved[ ii ]);
                        p_sys_fiod->outputs_minus[ ii ] &=~(p_app_fiod->outputs_reserved[ ii ]);
                }

		/* Isolate differences from what this APP has reserved */
		/* Only leave on bits that we want set on and have reserved */
		plus[ ii ]  &= p_app_fiod->outputs_reserved[ ii ];
		minus[ ii ] &= p_app_fiod->outputs_reserved[ ii ];

		/* Turn on our bits, that we are allowed to turn on */
		p_app_fiod->outputs_plus[ ii ]  |= plus[ ii ];
		p_app_fiod->outputs_minus[ ii ] |= minus[ ii ];
                if (!p_priv->transaction_in_progress) {
                        p_sys_fiod->outputs_plus[ ii ]  |= plus[ ii ];
                        p_sys_fiod->outputs_minus[ ii ] |= minus[ ii ];
                }
	}
	spin_unlock_irqrestore(&p_sys_fiod->lock, flags);
pr_debug("fioman_outputs_set: %x %x %x %x %x %x %x %x\n",
	plus[0]|minus[0],
	plus[1]|minus[1],
	plus[2]|minus[2],
	plus[3]|minus[3],
	plus[4]|minus[4],
	plus[5]|minus[5],
	plus[6]|minus[6],
	plus[7]|minus[7]);
	/* return success */
	return ( 0 );
}

/*****************************************************************************/

/*****************************************************************************/
/*
This function is used to get output points for an application.
*/
/*****************************************************************************/

int
fioman_outputs_get
(
	struct file			*filp,	/* File Pointer */
	FIO_IOC_OUTPUTS_GET	*p_arg	/* Arguments to process */
)
{
	FIOMAN_PRIV_DATA	*p_priv = filp->private_data;	/* Access Apps data */
	FIOMAN_APP_FIOD		*p_app_fiod;	/* Ptr to app fiod structure */
	u8			outputs_plus[FIO_OUTPUT_POINTS_BYTES];	/* Load Switch Plus to copy */
	u8			outputs_minus[FIO_OUTPUT_POINTS_BYTES];	/* Load Switch Minus to copy */
	unsigned long flags;

	/* Find this APP registeration */
	p_app_fiod = fioman_find_dev( p_priv, p_arg->dev_handle );

	/* See if we found the dev_handle */
	if ( NULL == p_app_fiod )
	{
		/* No, return error */
		return ( -EINVAL );
	}

	switch ( p_arg->view )
	{
		case FIO_VIEW_APP:	/* Return APP outputs */
		{
			memcpy(outputs_plus, p_app_fiod->outputs_plus, FIO_OUTPUT_POINTS_BYTES);
			memcpy(outputs_minus, p_app_fiod->outputs_minus, FIO_OUTPUT_POINTS_BYTES);
			break;
		}

		case FIO_VIEW_SYSTEM: /* Return FIOMAN outputs */
		{
			spin_lock_irqsave(&p_app_fiod->p_sys_fiod->lock, flags);
			memcpy(outputs_plus, p_app_fiod->p_sys_fiod->outputs_plus, FIO_OUTPUT_POINTS_BYTES);
			memcpy(outputs_minus, p_app_fiod->p_sys_fiod->outputs_minus, FIO_OUTPUT_POINTS_BYTES);
			spin_unlock_irqrestore(&p_app_fiod->p_sys_fiod->lock, flags);
pr_debug( "fioman_outputs_get: plus:%x %x %x %x %x %x %x %x %x %x %x %x %x\n",
	outputs_plus[0], outputs_plus[1], outputs_plus[2], outputs_plus[3],
	outputs_plus[4], outputs_plus[5], outputs_plus[6], outputs_plus[7],
	outputs_plus[8], outputs_plus[9], outputs_plus[10], outputs_plus[11],
	outputs_plus[12]);
			break;
		}

		default:
		{
			return ( -EINVAL );
		}
	}

	/* Copy Kernel Data array to User Space */
	if ( copy_to_user( p_arg->ls_plus, outputs_plus, FIO_OUTPUT_POINTS_BYTES ) )
	{
		/* Show error */
		return ( -EFAULT );
	}

	/* Copy Kernel Control array to User Space */
	if ( copy_to_user( p_arg->ls_minus, outputs_minus, FIO_OUTPUT_POINTS_BYTES ) )
	{
		/* Show error */
		return ( -EFAULT );
	}

	/* Show success */
	return ( 0 );
}

/*****************************************************************************/
int fioman_transaction_begin( struct file *filp )
{
        FIOMAN_PRIV_DATA *p_priv = filp->private_data;

        if (p_priv->transaction_in_progress)
                return -EINVAL;
                
        p_priv->transaction_in_progress = true;

        return 0;
}

int fioman_transaction_commit( struct file *filp )
{
        FIOMAN_PRIV_DATA *p_priv = filp->private_data;
	struct list_head *p_app_elem;
	FIOMAN_APP_FIOD  *p_app_fiod;
	FIOMAN_SYS_FIOD  *p_sys_fiod;
	int              ii;
	unsigned long    flags = 0;

        if ((p_priv == NULL) || (!p_priv->transaction_in_progress))
                return -EINVAL;
                
        /* Write all fiod outputs as one transaction */
        /* Get each of this app's sys_fiods, and lock */
        list_for_each( p_app_elem, &p_priv->fiod_list )
        {
		/* Get a ptr to this list entry */
		p_app_fiod = list_entry( p_app_elem, FIOMAN_APP_FIOD, elem );
		/* See if this FIOD is currently enabled */
		if ( p_app_fiod->enabled ) {
                        p_sys_fiod = p_app_fiod->p_sys_fiod;
                        /* Obtain lock for the associated sys_fiod */
                        spin_lock_irqsave(&p_sys_fiod->lock, flags);
		}
	}
        /* Copy from each app_fiod to each sys_fiod */
        list_for_each( p_app_elem, &p_priv->fiod_list )
        {
		/* Get a ptr to this list entry */
		p_app_fiod = list_entry( p_app_elem, FIOMAN_APP_FIOD, elem );
		/* See if this FIOD is currently enabled */
		if ( p_app_fiod->enabled ) {
                        p_sys_fiod = p_app_fiod->p_sys_fiod;
                        /* For each plus / minus byte */
                        for ( ii = 0; ii < FIO_OUTPUT_POINTS_BYTES; ii++ ) {
                                /* Turn off all bits we are allowed to touch */
                                p_sys_fiod->outputs_plus[ ii ]  &=~(p_app_fiod->outputs_reserved[ ii ]);
                                p_sys_fiod->outputs_plus[ ii ]  |= p_app_fiod->outputs_plus[ ii ];
                                /* Set any 1 bits (app_fiod outputs already masked against reserved */
                                p_sys_fiod->outputs_minus[ ii ] &=~(p_app_fiod->outputs_reserved[ ii ]);
                                p_sys_fiod->outputs_minus[ ii ] |= p_app_fiod->outputs_minus[ ii ];
                        }
		}
	}

        /* Unlock sys_fiods */
        list_for_each( p_app_elem, &p_priv->fiod_list )
        {
		/* Get a ptr to this list entry */
		p_app_fiod = list_entry( p_app_elem, FIOMAN_APP_FIOD, elem );
		/* See if this FIOD is currently enabled */
		if ( p_app_fiod->enabled ) {
                        p_sys_fiod = p_app_fiod->p_sys_fiod;
                        spin_unlock_irqrestore(&p_sys_fiod->lock, flags);
		}
	}
        
        p_priv->transaction_in_progress = false;
        
        return 0;
}

/*****************************************************************************/
/*
This function is used to reserve channels for an application.
*/
/*****************************************************************************/
int
fioman_channel_reserve_set
(
	struct file			*filp,	/* File Pointer */
	FIO_IOC_CHANNEL_RESERVE_SET	*p_arg	/* Arguments to process */
)
{
	FIOMAN_PRIV_DATA	*p_priv = filp->private_data;	/* Access Apps data */
	FIOMAN_APP_FIOD		*p_app_fiod;	/* Ptr to app fiod structure */
	FIOMAN_SYS_FIOD		*p_sys_fiod;	/* Ptr to FIOMAN fiod structure */
						/* Kernel buffers to use */
	u8 reserve[ FIO_CHANNEL_BYTES ] = {0};
	u8 relinquish[ FIO_CHANNEL_BYTES ] = {0};
	int ii;				/* Loop variable */

	/* Find this APP registration */
	p_app_fiod = fioman_find_dev( p_priv, p_arg->dev_handle );

	/* See if we found the dev_handle */
	if ( (NULL == p_app_fiod) || (p_arg->count > FIO_CHANNEL_BYTES) )
	{
		/* No, return error */
		return ( -EINVAL );
	}

	/* Copy APP bit array to Kernel Space */
	if ( copy_from_user( reserve, p_arg->data, p_arg->count ) )
	{
		/* failure */
		return ( -EFAULT );
	}

	pr_debug("reserving %d channel bytes=%0x %0x %0x %0x...\n",
		p_arg->count, reserve[0], reserve[1], reserve[2], reserve[3]);

	/* For each reservation byte */
	p_sys_fiod = p_app_fiod->p_sys_fiod;
	for ( ii = 0; ii < sizeof( reserve ); ii++ )
	{
		/* Save the complement for reliquishing channels */
		relinquish[ ii ] = ~( reserve[ ii ] );

		/* Isolate differences from what this APP has already reserved */
		reserve[ ii ] &= ~( p_app_fiod->channels_reserved[ ii ] );

		/* See if these diffs are ok with the FIOMAN list */
		if ( reserve[ ii ] & p_sys_fiod->channels_reserved[ ii ] )
		{
			/* If not, return error */
			return ( -ENOTTY );
		}
	}

	/* Set the masks accordingly */
	for ( ii = 0; ii < sizeof( reserve ); ii++ )
	{
		/* OR on bits in FIOMAN list */
		/* OR on bits in APP list */
		p_app_fiod->channels_reserved[ ii ] |= reserve[ ii ];
		p_sys_fiod->channels_reserved[ ii ] |= reserve[ ii ];

		/* Isolate differences from what this APP has already reserved */
		relinquish[ ii ] &= p_app_fiod->channels_reserved[ ii ];

		/* Turn off bits in APP and FIOMAN lists now */
		p_app_fiod->channels_reserved[ ii ] &= ~( relinquish[ ii ] );
		p_sys_fiod->channels_reserved[ ii ] &= ~( relinquish[ ii ] );
	}

	/* return success */
	return ( 0 );
}

/*****************************************************************************/
/*****************************************************************************/
/*
This function is used to return reserved channel status.
*/
/*****************************************************************************/
int
fioman_channel_reserve_get
(
	struct file			*filp,	/* File Pointer */
	FIO_IOC_CHANNEL_RESERVE_GET	*p_arg	/* Arguments to process */
)
{
	FIOMAN_PRIV_DATA	*p_priv = filp->private_data;	/* Access Apps data */
	FIOMAN_APP_FIOD		*p_app_fiod;	/* Ptr to app fiod structure */
	FIOMAN_SYS_FIOD		*p_sys_fiod;	/* Ptr to FIOMAN fiod structure */
						/* Kernel buffers to use */
	u8 reserve[ FIO_CHANNEL_BYTES ] = {0};
	int count = 0;
	int ii;				/* Loop variable */

	/* Find this APP registration */
	p_app_fiod = fioman_find_dev( p_priv, p_arg->dev_handle );

	/* See if we found the dev_handle */
	if ( (NULL == p_app_fiod) || (p_arg->count > FIO_CHANNEL_BYTES) ) {
		/* No, return error */
		return ( -EINVAL );
	}

	if ((p_app_fiod->fiod.fiod != FIOMMU) && (p_app_fiod->fiod.fiod != FIOCMU)) {
		return (-EINVAL);
	}

	p_sys_fiod = p_app_fiod->p_sys_fiod;
	for ( ii = 0; ii < sizeof(reserve); ii++ )
	{
		/* Is system view or app view requested? */
		if (p_arg->view == FIO_VIEW_SYSTEM)
			reserve[ii] = p_sys_fiod->channels_reserved[ii];
		else if (p_arg->view == FIO_VIEW_APP)
			reserve[ii] = p_app_fiod->channels_reserved[ii];
		else
			return (-EINVAL);
	}

	/* copy to user space */
	count = p_arg->count;
	if (count > sizeof(reserve))
		count = sizeof(reserve);
	if (count && (p_arg->data != NULL)) {
		if (copy_to_user(p_arg->data, reserve, count)) {
			/* Could not copy for some reason */
			return (-EFAULT);
		}
	}

	/* return success */
	return ( 0 );
}

/*****************************************************************************/
/*****************************************************************************/
/*
This function is used to set channel mappings for an application.
*/
/*****************************************************************************/
int
fioman_channel_map_set
(
	struct file				*filp,	/* File Pointer */
	FIO_IOC_CHAN_MAP_SET	*p_arg	/* Arguments to process */
)
{
	FIOMAN_PRIV_DATA	*p_priv = filp->private_data;	/* Access Apps data */
	FIOMAN_APP_FIOD		*p_app_fiod;	/* Ptr to app fiod structure */
	FIOMAN_APP_FIOD		*p_app_ref_fiod; /* Ptr to referenced app fiod */
	FIOMAN_SYS_FIOD		*p_sys_fiod;	/* Ptr to FIOMAN fiod structure */
	FIOMAN_SYS_FIOD		*p_sys_ref_fiod; /* Ptr to referenced sys fiod */
	struct list_head	*p_sys_elem;	/* Element from FIOMAN FIOD list */
	struct list_head	*p_next;		/* Temp for loop */
	FIO_CHANNEL_MAP		mappings[ FIO_CHANNELS*3 ];
	unsigned int		ii, count, channel, output;
	FIO_COLOR	color;

	/* Find this APP registration */
	p_app_fiod = fioman_find_dev( p_priv, p_arg->dev_handle );
	/* See if we found the dev_handle */
	if ( NULL == p_app_fiod ) {
		/* No, return error */
		return ( -EINVAL );
	}

	if ((p_app_fiod->fiod.fiod != FIOMMU) && (p_app_fiod->fiod.fiod != FIOCMU)) {
		return (-EINVAL);
	}

	/* Copy APP data array to Kernel Space */
	count = (p_arg->count < (FIO_CHANNELS*3))?p_arg->count:(FIO_CHANNELS*3);
	if ( copy_from_user( mappings, p_arg->map, count*sizeof(FIO_CHANNEL_MAP) ) ) {
		/* Could not copy for some reason */
		return ( -EFAULT );
	}

	/* Check channel and output ownership of all mappings */
	/* For each mapping */
	for ( ii = 0; ii < count; ii++ )
	{
		channel = mappings[ii].channel;
		output = mappings[ii].output_point;
		color = mappings[ii].color;
		if ((channel >= FIO_CHANNELS) || (output >= FIO_OUTPUT_POINTS_BYTES*8)
				|| (color > FIO_RED))
			return (-EINVAL);
		if ( FIO_BIT_TEST(p_app_fiod->channels_reserved, channel) == 0 )
			return (-EPERM);
		if ((p_app_ref_fiod = fioman_find_dev(p_priv, mappings[ii].fiod)) == NULL )
			return (-EINVAL);
		if( FIO_BIT_TEST(p_app_ref_fiod->outputs_reserved, output) == 0 )
			return (-EPERM);
	}

	/* Clear old mappings first */
	for (ii = 0; ii < FIO_CHANNELS; ii++) {
		if (FIO_BIT_TEST(p_app_fiod->channels_reserved, ii)) {
			/* for each device */
			list_for_each_safe( p_sys_elem, p_next, &fioman_fiod_list )
			{
				/* Get a ptr to this list entry */
				p_sys_fiod = list_entry( p_sys_elem, FIOMAN_SYS_FIOD, elem );
				/* TBD: check for relevant (output) device type? TFBIU, SIU?*/
				/* clear output for green/yellow/red channel */
				p_sys_fiod->channel_map_green[ii] = 0;
				p_sys_fiod->channel_map_yellow[ii] = 0;
				p_sys_fiod->channel_map_red[ii] = 0;
			}
		}
	}
	/* Create new mappings */
	/* For each mapping */
	for ( ii = 0; ii < count; ii++ )
	{
		channel = mappings[ii].channel;
		output = mappings[ii].output_point;
		p_app_ref_fiod = fioman_find_dev(p_priv, mappings[ii].fiod);
		p_sys_ref_fiod = p_app_ref_fiod->p_sys_fiod;
		switch( mappings[ii].color ) {
		case FIO_GREEN:
			p_sys_ref_fiod->channel_map_green[channel] = output+1;
			break;
		case FIO_YELLOW:
			p_sys_ref_fiod->channel_map_yellow[channel] = output+1;
			break;
		case FIO_RED:
			p_sys_ref_fiod->channel_map_red[channel] = output+1;
			break;
		default:
			return(-EINVAL);
		}
	}

	/* return success */
	return ( 0 );
}

/*****************************************************************************/

/*****************************************************************************/
/*
This function is used to get channel mappings for an application.
*/
/*****************************************************************************/
int
fioman_channel_map_get
(
	struct file				*filp,	/* File Pointer */
	FIO_IOC_CHAN_MAP_GET	*p_arg	/* Arguments to process */
)
{
	FIOMAN_PRIV_DATA	*p_priv = filp->private_data;	/* Access Apps data */
	FIOMAN_APP_FIOD		*p_app_fiod;	/* Ptr to app fiod structure */
	FIOMAN_SYS_FIOD		*p_sys_fiod;	/* Ptr to FIOMAN fiod structure */
	FIOMAN_SYS_FIOD		*p_sys_ref_fiod; /* Ptr to referenced sys fiod */
	struct list_head	*p_sys_elem;	/* Element from FIOMAN FIOD list */
	struct list_head	*p_next;		/* Temp for loop */
	FIO_CHANNEL_MAP		mappings[ FIO_CHANNELS ];
	unsigned int		ii, count = 0;

	/* Find this APP registration */
	p_app_fiod = fioman_find_dev( p_priv, p_arg->dev_handle );
	/* See if we found the dev_handle */
	if ( NULL == p_app_fiod ) {
		/* No, return error */
		return ( -EINVAL );
	}

	if ((p_app_fiod->fiod.fiod != FIOMMU) && (p_app_fiod->fiod.fiod != FIOCMU)) {
		return (-EINVAL);
	}

	/* Is system view or app view requested? */
	if (p_arg->view == FIO_VIEW_SYSTEM) {
		p_sys_ref_fiod = p_app_fiod->p_sys_fiod;
		for (ii = 0; (ii < FIO_CHANNELS) && (count < p_arg->count); ii++) {
			if (FIO_BIT_TEST(p_sys_ref_fiod->channels_reserved, ii)) {
				/*if mapped*/
				list_for_each_safe( p_sys_elem, p_next, &fioman_fiod_list )
				{
					/* Get a ptr to this list entry */
					p_sys_fiod = list_entry( p_sys_elem, FIOMAN_SYS_FIOD, elem );
					if (p_sys_fiod->channel_map_green[ii] > 0) {
						mappings[count].output_point = p_sys_fiod->channel_map_green[ii] - 1;
						mappings[count].color = FIO_GREEN;
					} else if (p_sys_fiod->channel_map_yellow[ii] > 0) {
						mappings[count].output_point = p_sys_fiod->channel_map_yellow[ii] - 1;
						mappings[count].color = FIO_YELLOW;
					} else if (p_sys_fiod->channel_map_red[ii] > 0) {
						mappings[count].output_point = p_sys_fiod->channel_map_red[ii] - 1;
						mappings[count].color = FIO_RED;
					} else {
						/* Not mapped to this fiod */
						continue;
					}
					mappings[count].channel = ii;
					mappings[count].fiod = p_sys_fiod->dev_handle;
					count++;
				}
			}
		}
	} else if (p_arg->view == FIO_VIEW_APP) {
		for (ii = 0; ii < FIO_CHANNELS; ii++) {
			if (FIO_BIT_TEST(p_app_fiod->channels_reserved, ii)) {
				/*if mapped*/
				list_for_each_safe( p_sys_elem, p_next, &fioman_fiod_list )
				{
					/* Get a ptr to this list entry */
					p_sys_fiod = list_entry( p_sys_elem, FIOMAN_SYS_FIOD, elem );
					if (p_sys_fiod->channel_map_green[ii] > 0) {
						mappings[count].output_point = p_sys_fiod->channel_map_green[ii] - 1;
						mappings[count].color = FIO_GREEN;
					} else if (p_sys_fiod->channel_map_yellow[ii] > 0) {
						mappings[count].output_point = p_sys_fiod->channel_map_yellow[ii] - 1;
						mappings[count].color = FIO_YELLOW;
					} else if (p_sys_fiod->channel_map_red[ii] > 0) {
						mappings[count].output_point = p_sys_fiod->channel_map_red[ii] - 1;
						mappings[count].color = FIO_RED;
					} else {
						/* Not mapped to this fiod */
						continue;
					}
					mappings[count].channel = ii;
					mappings[count].fiod = p_sys_fiod->dev_handle;
					count++;
				}
			}
		}
	} else {
		/* error, invalid view specified */
		return(-EINVAL);
	}

	/* copy to user space */
	if (count && (p_arg->map != NULL))
		if (copy_to_user(p_arg->map, mappings, count*sizeof(FIO_CHANNEL_MAP))) {
			/* Could not copy for some reason */
			return ( -EFAULT );
		}

	/* Show success */
	return ( count );
}

/*****************************************************************************/

/*****************************************************************************/
/*
This function is used to get a count of channel mappings for an application.
*/
/*****************************************************************************/

int
fioman_channel_map_count
(
	struct file				*filp,	/* File Pointer */
	FIO_IOC_CHAN_MAP_COUNT	*p_arg	/* Arguments to process */
)
{
	FIOMAN_PRIV_DATA	*p_priv = filp->private_data;	/* Access Apps data */
	FIOMAN_APP_FIOD		*p_app_fiod;	/* Ptr to app fiod structure */
	FIOMAN_SYS_FIOD		*p_sys_fiod;	/* Ptr to FIOMAN fiod structure */
	FIOMAN_SYS_FIOD		*p_sys_ref_fiod; /* Ptr to referenced sys fiod */
	struct list_head	*p_sys_elem;	/* Element from FIOMAN FIOD list */
	struct list_head	*p_next;		/* Temp for loop */
	unsigned int		ii, count = 0;

	/* Find this APP registration */
	p_app_fiod = fioman_find_dev( p_priv, p_arg->dev_handle );
	/* See if we found the dev_handle */
	if ( NULL == p_app_fiod ) {
		/* No, return error */
		return ( -EINVAL );
	}

	if ((p_app_fiod->fiod.fiod != FIOMMU) && (p_app_fiod->fiod.fiod != FIOCMU)) {
		return (-EINVAL);
	}

	/* Is system view or app view requested? */
	if (p_arg->view == FIO_VIEW_SYSTEM) {
		p_sys_ref_fiod = p_app_fiod->p_sys_fiod;
		for (ii = 0; ii < FIO_CHANNELS; ii++) {
			if (FIO_BIT_TEST(p_sys_ref_fiod->channels_reserved, ii)) {
				/*if mapped*/
				list_for_each_safe( p_sys_elem, p_next, &fioman_fiod_list )
				{
					/* Get a ptr to this list entry */
					p_sys_fiod = list_entry( p_sys_elem, FIOMAN_SYS_FIOD, elem );
					if (p_sys_fiod->channel_map_green[ii] > 0)
						count++;
					if (p_sys_fiod->channel_map_yellow[ii] > 0)
						count++;
					if (p_sys_fiod->channel_map_red[ii] > 0)
						count++;
				}
			}
		}
	} else if (p_arg->view == FIO_VIEW_APP) {
		for (ii = 0; ii < FIO_CHANNELS; ii++) {
			if (FIO_BIT_TEST(p_app_fiod->channels_reserved, ii)) {
				/*if mapped*/
				list_for_each_safe( p_sys_elem, p_next, &fioman_fiod_list )
				{
					/* Get a ptr to this list entry */
					p_sys_fiod = list_entry( p_sys_elem, FIOMAN_SYS_FIOD, elem );
					if (p_sys_fiod->channel_map_green[ii] > 0)
						count++;
					if (p_sys_fiod->channel_map_yellow[ii] > 0)
						count++;
					if (p_sys_fiod->channel_map_red[ii] > 0)
						count++;
				}
			}
		}
	} else {
		/* error, invalid view specified */
		return(-EINVAL);
	}
	/* Show success */
	return ( count );
}

/*****************************************************************************/

/*****************************************************************************/
/*
This function is used to set the mmu flash bit state.
*/
/*****************************************************************************/

int
fioman_mmu_flash_bit_set
(
	struct file			*filp,	/* File Pointer */
	FIO_IOC_MMU_FLASH_BIT_SET	*p_arg	/* Arguments to process */
)
{
	FIOMAN_PRIV_DATA	*p_priv = filp->private_data;	/* Access Apps data */
	FIOMAN_APP_FIOD		*p_app_fiod;	/* Ptr to app fiod structure */
	FIOMAN_SYS_FIOD		*p_sys_fiod;	/* Ptr to FIOMAN fiod structure */

	/* Find this APP registration */
	p_app_fiod = fioman_find_dev( p_priv, p_arg->dev_handle );
	/* See if we found the dev_handle */
	if ( NULL == p_app_fiod ) {
		/* No, return error */
		return ( -EINVAL );
	}

	if (p_app_fiod->fiod.fiod != FIOMMU) {
		return (-EINVAL);
	}

	p_sys_fiod = p_app_fiod->p_sys_fiod;
	if (p_arg->fb == FIO_MMU_FLASH_BIT_ON)
		p_sys_fiod->flash_bit = p_app_fiod->flash_bit = FIO_MMU_FLASH_BIT_ON;
	else if (p_arg->fb == FIO_MMU_FLASH_BIT_OFF)
		p_sys_fiod->flash_bit = p_app_fiod->flash_bit = FIO_MMU_FLASH_BIT_OFF;
	else
		return (-EINVAL);

	return 0;
}

/*****************************************************************************/
/*
This function is used to get the mmu flash bit state.
*/
/*****************************************************************************/

int
fioman_mmu_flash_bit_get
(
	struct file			*filp,	/* File Pointer */
	FIO_IOC_MMU_FLASH_BIT_GET	*p_arg	/* Arguments to process */
)
{
	FIOMAN_PRIV_DATA	*p_priv = filp->private_data;	/* Access Apps data */
	FIOMAN_APP_FIOD		*p_app_fiod;	/* Ptr to app fiod structure */
	FIOMAN_SYS_FIOD		*p_sys_fiod;	/* Ptr to FIOMAN fiod structure */

	/* Find this APP registration */
	p_app_fiod = fioman_find_dev( p_priv, p_arg->dev_handle );
	/* See if we found the dev_handle */
	if ( NULL == p_app_fiod ) {
		/* No, return error */
		return ( -EINVAL );
	}

	if (p_app_fiod->fiod.fiod != FIOMMU) {
		return (-EINVAL);
	}
	if (p_arg->fb == NULL)
		return(-EINVAL);

	/* Is system view or app view requested? */
	if (p_arg->view == FIO_VIEW_SYSTEM) {
		p_sys_fiod = p_app_fiod->p_sys_fiod;
		put_user(p_sys_fiod->flash_bit, p_arg->fb);
	} else if (p_arg->view == FIO_VIEW_APP) {
		put_user(p_app_fiod->flash_bit, p_arg->fb);
	} else {
		/* error, invalid view specified */
		return(-EINVAL);
	}

	return 0;
}


/*****************************************************************************/
/*
This function is used to set the TS fault monitor state.
*/
/*****************************************************************************/

int
fioman_ts_fault_monitor_set
(
	struct file			*filp,	/* File Pointer */
	FIO_IOC_TS_FMS_SET		*p_arg	/* Arguments to process */
)
{
	FIOMAN_PRIV_DATA	*p_priv = filp->private_data;	/* Access Apps data */
	FIOMAN_APP_FIOD		*p_app_fiod;	/* Ptr to app fiod structure */
	FIOMAN_SYS_FIOD		*p_sys_fiod;	/* Ptr to FIOMAN fiod structure */

	/* Find this APP registration */
	p_app_fiod = fioman_find_dev( p_priv, p_arg->dev_handle );
	/* See if we found the dev_handle */
	if ( NULL == p_app_fiod ) {
		/* No, return error */
		return ( -EINVAL );
	}

	if ((p_app_fiod->fiod.fiod != FIOTS1) && (p_app_fiod->fiod.fiod != FIOTS2)) {
		return (-EINVAL);
	}

	p_sys_fiod = p_app_fiod->p_sys_fiod;
	if (p_arg->fms == FIO_TS_FM_ON)
		p_sys_fiod->fm_state = p_app_fiod->fm_state = FIO_TS_FM_ON;
	else if (p_arg->fms == FIO_TS_FM_OFF)
		p_sys_fiod->fm_state = p_app_fiod->fm_state = FIO_TS_FM_OFF;
	else
		return (-EINVAL);

pr_debug("fioman_ts_fault_monitor_set: %d\n", p_sys_fiod->fm_state);
#ifdef FAULTMON_GPIO
        if (faultmon_gpio != -1) {
                if (gpio_cansleep(faultmon_gpio))
                        gpio_set_value_cansleep(faultmon_gpio, p_sys_fiod->fm_state?1:0);
                else
                        gpio_set_value(faultmon_gpio, p_sys_fiod->fm_state?1:0);
        }
#endif
                
	return 0;
}

/*****************************************************************************/
/*
This function is used to get the ts fault monitor state.
*/
/*****************************************************************************/

int
fioman_ts_fault_monitor_get
(
	struct file		*filp,	/* File Pointer */
	FIO_IOC_TS_FMS_GET	*p_arg	/* Arguments to process */
)
{
	FIOMAN_PRIV_DATA	*p_priv = filp->private_data;	/* Access Apps data */
	FIOMAN_APP_FIOD		*p_app_fiod;	/* Ptr to app fiod structure */
	FIOMAN_SYS_FIOD		*p_sys_fiod;	/* Ptr to FIOMAN fiod structure */

	/* Find this APP registration */
	p_app_fiod = fioman_find_dev( p_priv, p_arg->dev_handle );
	/* See if we found the dev_handle */
	if ( NULL == p_app_fiod ) {
		/* No, return error */
		return ( -EINVAL );
	}

	if ((p_app_fiod->fiod.fiod != FIOTS1) && (p_app_fiod->fiod.fiod != FIOTS2)) {
		return (-EINVAL);
	}
	if (p_arg->fms == NULL)
		return(-EINVAL);

	/* Is system view or app view requested? */
	if (p_arg->view == FIO_VIEW_SYSTEM) {
		p_sys_fiod = p_app_fiod->p_sys_fiod;
		put_user(p_sys_fiod->fm_state, p_arg->fms);
	} else if (p_arg->view == FIO_VIEW_APP) {
		put_user(p_app_fiod->fm_state, p_arg->fms);
	} else {
		/* error, invalid view specified */
		return(-EINVAL);
	}

	return 0;
}

/*****************************************************************************/
/*
This function is used to get the cmu config change count.
*/
/*****************************************************************************/

int
fioman_cmu_config_change_count
(
	struct file		*filp,	/* File Pointer */
	FIO_DEV_HANDLE		dev_handle	/* Arguments to process */
)
{
	FIOMAN_PRIV_DATA	*p_priv = filp->private_data;	/* Access Apps data */
	FIOMAN_APP_FIOD		*p_app_fiod;	/* Ptr to app fiod structure */
	FIOMAN_SYS_FIOD		*p_sys_fiod;	/* Ptr to FIOMAN fiod structure */

	/* Find this APP registration */
	p_app_fiod = fioman_find_dev( p_priv, dev_handle );
	/* See if we found the dev_handle */
	if ( NULL == p_app_fiod ) {
		/* No, return error */
		return ( -EINVAL );
	}

	if (p_app_fiod->fiod.fiod != FIOCMU) {
		return (-EINVAL);
	}

	p_sys_fiod = p_app_fiod->p_sys_fiod;
	return (p_sys_fiod->cmu_config_change_count);
}

/*****************************************************************************/
/*
This function is used to set the CMU fault action state.
*/
/*****************************************************************************/

int
fioman_cmu_fault_set
(
	struct file			*filp,	/* File Pointer */
	FIO_IOC_CMU_FSA_SET		*p_arg	/* Arguments to process */
)
{
	FIOMAN_PRIV_DATA	*p_priv = filp->private_data;	/* Access Apps data */
	FIOMAN_APP_FIOD		*p_app_fiod;	/* Ptr to app fiod structure */
	FIOMAN_SYS_FIOD		*p_sys_fiod;	/* Ptr to FIOMAN fiod structure */
	struct list_head	*p_app_elem;	/* Ptr to app element being examined */
	FIO_CMU_FSA		cmu_fsa;

	/* Find this APP registration */
	p_app_fiod = fioman_find_dev( p_priv, p_arg->dev_handle );
	/* See if we found the dev_handle */
	if ( NULL == p_app_fiod ) {
		/* No, return error */
		return ( -EINVAL );
	}

	if (p_app_fiod->fiod.fiod != FIOCMU) {
		return (-EINVAL);
	}

	if ((p_arg->fsa < FIO_CMU_FSA_NONE) || (p_arg->fsa > FIO_CMU_FSA_LATCHING)) 
		return (-EINVAL);

	p_app_fiod->cmu_fsa = p_arg->fsa;
	
	p_sys_fiod = p_app_fiod->p_sys_fiod;
	
	/* set system fsa to highest of all app fsa values for registered apps */
	p_app_fiod = NULL;
	cmu_fsa = FIO_CMU_FSA_NONE;
	list_for_each( p_app_elem, &p_sys_fiod->app_fiod_list )
	{
		/* Get a ptr to this list entry */
		p_app_fiod = list_entry( p_app_elem, FIOMAN_APP_FIOD, sys_elem );
		if (cmu_fsa < p_app_fiod->cmu_fsa)
			cmu_fsa = p_app_fiod->cmu_fsa;
	}
	p_sys_fiod->cmu_fsa = cmu_fsa;

	return 0;
}

/*****************************************************************************/
/*
This function is used to get the cmu fault action state.
*/
/*****************************************************************************/

int
fioman_cmu_fault_get
(
	struct file		*filp,	/* File Pointer */
	FIO_IOC_CMU_FSA_GET	*p_arg	/* Arguments to process */
)
{
	FIOMAN_PRIV_DATA	*p_priv = filp->private_data;	/* Access Apps data */
	FIOMAN_APP_FIOD		*p_app_fiod;	/* Ptr to app fiod structure */
	FIOMAN_SYS_FIOD		*p_sys_fiod;	/* Ptr to FIOMAN fiod structure */

	/* Find this APP registration */
	p_app_fiod = fioman_find_dev( p_priv, p_arg->dev_handle );
	/* See if we found the dev_handle */
	if ( NULL == p_app_fiod ) {
		/* No, return error */
		return ( -EINVAL );
	}

	if (p_app_fiod->fiod.fiod != FIOCMU) {
		return (-EINVAL);
	}
	if (p_arg->fsa == NULL)
		return(-EINVAL);

	/* Is system view or app view requested? */
	if (p_arg->view == FIO_VIEW_SYSTEM) {
		p_sys_fiod = p_app_fiod->p_sys_fiod;
		put_user(p_sys_fiod->cmu_fsa, p_arg->fsa);
	} else if (p_arg->view == FIO_VIEW_APP) {
		put_user(p_app_fiod->cmu_fsa, p_arg->fsa);
	} else {
		/* error, invalid view specified */
		return(-EINVAL);
	}

	return 0;
}

/*****************************************************************************/
/*
This function is used to set the CMU dark channel mask.
*/
/*****************************************************************************/

int
fioman_cmu_dark_channel_set
(
	struct file			*filp,	/* File Pointer */
	FIO_IOC_CMU_DC_SET		*p_arg	/* Arguments to process */
)
{
	FIOMAN_PRIV_DATA	*p_priv = filp->private_data;	/* Access Apps data */
	FIOMAN_APP_FIOD		*p_app_fiod;	/* Ptr to app fiod structure */
	FIOMAN_SYS_FIOD		*p_sys_fiod;	/* Ptr to FIOMAN fiod structure */

	/* Find this APP registration */
	p_app_fiod = fioman_find_dev( p_priv, p_arg->dev_handle );
	/* See if we found the dev_handle */
	if ( NULL == p_app_fiod ) {
		/* No, return error */
		return ( -EINVAL );
	}

	if (p_app_fiod->fiod.fiod != FIOCMU) {
		return (-EINVAL);
	}

	if ((p_arg->mask < FIO_CMU_DC_MASK1) || (p_arg->mask > FIO_CMU_DC_MASK4)) 
		return (-EINVAL);

	p_app_fiod->cmu_mask = p_arg->mask;

	/* set system mask to latest value */
	p_sys_fiod = p_app_fiod->p_sys_fiod;
	p_sys_fiod->cmu_mask = p_app_fiod->cmu_mask;

	return 0;
}

/*****************************************************************************/
/*
This function is used to get the cmu dark channel mask.
*/
/*****************************************************************************/

int
fioman_cmu_dark_channel_get
(
	struct file		*filp,	/* File Pointer */
	FIO_IOC_CMU_DC_GET	*p_arg	/* Arguments to process */
)
{
	FIOMAN_PRIV_DATA	*p_priv = filp->private_data;	/* Access Apps data */
	FIOMAN_APP_FIOD		*p_app_fiod;	/* Ptr to app fiod structure */
	FIOMAN_SYS_FIOD		*p_sys_fiod;	/* Ptr to FIOMAN fiod structure */

	/* Find this APP registration */
	p_app_fiod = fioman_find_dev( p_priv, p_arg->dev_handle );
	/* See if we found the dev_handle */
	if ( NULL == p_app_fiod ) {
		/* No, return error */
		return ( -EINVAL );
	}

	if (p_app_fiod->fiod.fiod != FIOCMU) {
		return (-EINVAL);
	}
	if (p_arg->mask == NULL)
		return(-EINVAL);

	/* Is system view or app view requested? */
	if (p_arg->view == FIO_VIEW_SYSTEM) {
		p_sys_fiod = p_app_fiod->p_sys_fiod;
		put_user(p_sys_fiod->cmu_mask, p_arg->mask);
	} else if (p_arg->view == FIO_VIEW_APP) {
		put_user(p_app_fiod->cmu_mask, p_arg->mask);
	} else {
		/* error, invalid view specified */
		return(-EINVAL);
	}

	return 0;
}

/*****************************************************************************/
/*
This function is used to get input points for an application.
*/
/*****************************************************************************/

int
fioman_inputs_get
(
	struct file		*filp,	/* File Pointer */
	FIO_IOC_INPUTS_GET	*p_arg	/* Arguments to process */
)
{
	FIOMAN_PRIV_DATA	*p_priv = filp->private_data;	/* Access Apps data */
	FIOMAN_APP_FIOD		*p_app_fiod;			/* Ptr to app fiod structure */
	FIOMAN_SYS_FIOD		*p_sys_fiod;			/* Ptr to FIOMAN fiod structure */
	u8			inputs[ FIO_INPUT_POINTS_BYTES ];
	int count = 0, err = 0;
	unsigned long flags;

        /* Adjust byte count to buffer size */
        count = p_arg->num_data_bytes;
	if (count <= 0)
		/* No buffer to copy inputs */
		return -EINVAL;

        if (count > FIO_INPUT_POINTS_BYTES)
                count = FIO_INPUT_POINTS_BYTES;

	/* Find this APP registration */
	p_app_fiod = fioman_find_dev( p_priv, p_arg->dev_handle );

	/* See if we found the dev_handle */
	if ( NULL == p_app_fiod )
		/* No, return error */
		return -EINVAL;
		
	p_sys_fiod = p_app_fiod->p_sys_fiod;
	spin_lock_irqsave(&p_sys_fiod->lock, flags);
	switch ( p_arg->inputs_type )
	{
		case FIO_INPUTS_RAW:	/* Return unfiltered inputs */
		{
			/* Does this fiod support raw inputs frame? */
			if (p_sys_fiod->frame_frequency_table[FIOMAN_FRAME_NO_52] == -1) {
				/* No, copy raw inputs as only type supported */
			memcpy(inputs, p_sys_fiod->inputs_raw, count);
                        } else {
				/* Is the appropriate frame scheduled? */
				if (p_sys_fiod->frame_frequency_table[FIOMAN_FRAME_NO_52] > FIO_HZ_0) {
					/* Copy raw inputs */
					memcpy(inputs, p_sys_fiod->inputs_raw, count);
				} else {
					/* Set inputs to zero */
					memset(inputs, 0, count);
				}
			}
			break;
		}

		case FIO_INPUTS_FILTERED: /* Return filtered inputs */
		{
			/* Does this fiod support filtered inputs frame? */
			if (p_sys_fiod->frame_frequency_table[FIOMAN_FRAME_NO_53] == -1) {
				/* No, copy raw inputs as only type supported */
				memcpy(inputs, p_sys_fiod->inputs_raw, count);
                        } else {
				/* Is the appropriate frame scheduled? */
				if (p_sys_fiod->frame_frequency_table[FIOMAN_FRAME_NO_53] > FIO_HZ_0) {
					/* Copy filtered inputs */
					memcpy(inputs, p_sys_fiod->inputs_filtered, count);
				} else {
					/* Set inputs to zero */
					memset(inputs, 0, count);
				}
			}
			break;
		}
		default:
			err = -EINVAL;
	}
	spin_unlock_irqrestore(&p_sys_fiod->lock, flags);

        /*pr_debug( KERN_ALERT "fiod_inputs_get: fiod(%d), %x %x %x %x %x %x %x %x\n",
                p_sys_fiod->fiod.fiod,
		inputs[0], inputs[1], inputs[2], inputs[3],
		inputs[4], inputs[5], inputs[6], inputs[7]);*/
		
	/* Copy Kernel Data array to User Space */
	if (err == 0)
                if (copy_to_user(p_arg->data, inputs, count))
                        /* Show error */
                        err = -EFAULT;

	return err;
}

/*****************************************************************************/

/*****************************************************************************/
/*
This function is used to get the fiod status.
*/
/*****************************************************************************/

int
fioman_fiod_status_get
(
	struct file                *filp, /* File Pointer */
	FIO_IOC_FIOD_STATUS        __user *p_arg /* Arguments to process */
)
{
	FIOMAN_PRIV_DATA      *p_priv = filp->private_data; /* Access Apps data */
	FIOMAN_APP_FIOD       *p_app_fiod;	/* Ptr to app fiod structure */
	FIOMAN_SYS_FIOD       *p_sys_fiod;	/* Ptr to FIOMAN fiod structure */
	FIOMSG_PORT           *p_port;
	FIO_FIOD_STATUS       __user *p_usr_status = p_arg->status;
	FIO_FRAME_INFO        __user *p_usr_info = p_usr_status->frame_info;
	struct list_head      *p_elem;		/* Ptr to list element */
	struct list_head      *p_next;		/* Temp for safe loop */
	FIOMSG_RX_FRAME       *p_rx_frame;	/* Ptr to rx frame being examined */
	unsigned int          index = 0;
	uint32_t              success_rx = 0, error_rx = 0;
	
	/* Find this APP registration */
	p_app_fiod = fioman_find_dev( p_priv, p_arg->dev_handle );
	/* See if we found the dev_handle */
	if (NULL == p_app_fiod) {
		/* No, return error */
		return ( -EINVAL );
	}

	if (NULL == p_usr_status)
		return (-EINVAL);

	p_sys_fiod = p_app_fiod->p_sys_fiod;

	/* For each element in the list */
	p_port = FIOMSG_P_PORT( p_sys_fiod->fiod.port );
	list_for_each_safe( p_elem, p_next, &p_port->rx_fiod_list[ p_sys_fiod->fiod.fiod ] )
	{
		/* Get the response frame for this queue element */
		p_rx_frame = list_entry( p_elem, FIOMSG_RX_FRAME, elem );

		/* Update fiod cummulative totals */
		if (success_rx < (424294967295L - p_rx_frame->info.success_rx))
			success_rx += p_rx_frame->info.success_rx;
		else
			success_rx = 424294967295L;
			
		if (error_rx < (424294967295L - p_rx_frame->info.error_rx))
			error_rx += p_rx_frame->info.error_rx;
		else
			error_rx = 424294967295L;
		
		/* Update corresponding frame info in response */
		index = FIOMSG_PAYLOAD( p_rx_frame )->frame_no - 128;
		pr_debug( "fiod_status_get: fiod(%d), frame(%d), seq(%u), err(%u)(%u)\n",
			p_sys_fiod->fiod.fiod, index+128, p_rx_frame->info.last_seq,
			p_rx_frame->info.error_rx, p_rx_frame->info.error_last_10);
		if (copy_to_user(&p_usr_info[index], &p_rx_frame->info,	sizeof(FIO_FRAME_INFO))) {
			/* Could not copy for some reason */
			return ( -EFAULT );
		}
	}
	put_user(p_app_fiod->enabled, &p_usr_status->comm_enabled);
	put_user(success_rx, &p_usr_status->success_rx);
	put_user(error_rx, &p_usr_status->error_rx);

	/* Show success */
	return ( 0 );
}

/*****************************************************************************/

/*****************************************************************************/
/*
Function to reset FIOD status counts.
*/
/*****************************************************************************/

int
fioman_fiod_status_reset
(
	struct file	*filp,		/* File Pointer */
	FIO_DEV_HANDLE	dev_handle	/* Previously returned dev_handle */
)
{
	FIOMAN_PRIV_DATA	*p_priv = filp->private_data;	/* Access Apps data */
	FIOMAN_APP_FIOD		*p_app_fiod;	/* Ptr to app fiod structure */
	FIOMAN_SYS_FIOD		*p_sys_fiod;	/* Ptr to FIOMAN fiod structure */
	FIOMSG_PORT		*p_port;
	struct list_head	*p_elem;		/* Ptr to list element */
	struct list_head	*p_next;		/* Temp for safe loop */
	FIOMSG_RX_FRAME		*p_rx_frame;	/* Ptr to rx frame being examined */

	/* Find this APP registration */
	p_app_fiod = fioman_find_dev( p_priv, dev_handle );

	/* See if we found the dev_handle */
	if ( NULL == p_app_fiod )
	{
		/* No, return error */
		return ( -EINVAL );
	}

	p_sys_fiod = p_app_fiod->p_sys_fiod;
	if (p_app_fiod->enabled) {
		p_sys_fiod->success_rx = 0;
		p_sys_fiod->error_rx = 0;
		
		/* For each element in the receive frame list */
		p_port = FIOMSG_P_PORT( p_sys_fiod->fiod.port );
		list_for_each_safe( p_elem, p_next, &p_port->rx_fiod_list[ p_sys_fiod->fiod.fiod ] )
		{
			/* Get the response frame for this queue element */
			p_rx_frame = list_entry( p_elem, FIOMSG_RX_FRAME, elem );
			p_rx_frame->info.error_rx = 0;
			p_rx_frame->info.success_rx = 0;
			p_rx_frame->info.error_last_10 = 0;
		}
	}

	return 0;
}

/*****************************************************************************/

/*****************************************************************************/
/*
This function is used to read response frame data.
*/
/*****************************************************************************/

int fioman_fiod_frame_read
(
	struct file              *filp, /* File Pointer */
	FIO_IOC_FIOD_FRAME_READ  *p_arg /* Arguments to process */
)
{
	FIOMAN_PRIV_DATA *p_priv = filp->private_data; /* Access Apps data */
	FIOMAN_APP_FIOD  *p_app_fiod;                  /* Ptr to app fiod structure */
	FIOMSG_PORT      *p_port;
	struct list_head *p_elem;                      /* Ptr to list element */
	struct list_head *p_next;                      /* Temp for safe loop */
	FIOMSG_RX_FRAME  *p_rx_frame;                  /* Ptr to rx frame being examined */
	unsigned int     count = 0;
        int ret;

	/* Find this APP registration */
	p_app_fiod = fioman_find_dev( p_priv, p_arg->dev_handle );
	/* See if we found the dev_handle */
	if (NULL == p_app_fiod) {
		/* No, return error */
		return ( -EINVAL );
	}

	if ((NULL == p_arg->buf) || (p_arg->timeout < 0))
		return -EINVAL;

	/* For each element in the list */
	p_port = FIOMSG_P_PORT( p_app_fiod->fiod.port );
	list_for_each_safe( p_elem, p_next, &p_port->rx_fiod_list[ p_app_fiod->fiod.fiod ] )
	{
		/* Get the response frame for this queue element */
		p_rx_frame = list_entry( p_elem, FIOMSG_RX_FRAME, elem );

		/* See if current element frame no is what we are looking for */
		if (FIOMSG_PAYLOAD( p_rx_frame )->frame_no == p_arg->rx_frame) {
                        if (p_rx_frame->resp == false) {
                                ret = wait_event_interruptible_timeout(p_port->read_wait,
                                        (p_rx_frame->resp == true), msecs_to_jiffies(p_arg->timeout));
                                if (ret == 0)
                                        return -ETIMEDOUT;
                        }
                        count = ((p_rx_frame->len - 2) < p_arg->count) ? (p_rx_frame->len - 2) : p_arg->count;
                        if (copy_to_user(p_arg->buf, &FIOMSG_PAYLOAD(p_rx_frame)->frame_no, count )) {
                                /* Could not copy for some reason */
                                return ( -EFAULT );
                        }
                        /* Update other frame info in response */
                        if (p_arg->seq_number != NULL)
                                put_user(p_rx_frame->info.last_seq, p_arg->seq_number);
                        return (count);
		}
	}

	/* Show invalid frame number */
	return (-EINVAL);
}

/*****************************************************************************/
/*
This function is used to request the sending of an arbitrary command frame.
*/
/*****************************************************************************/

int
fioman_fiod_frame_write
(
	struct file			*filp,	/* File Pointer */
	FIO_IOC_FIOD_FRAME_WRITE	*p_arg	/* Arguments to process */
)
{
	FIOMAN_PRIV_DATA	*p_priv = filp->private_data;	/* Access Apps data */
	FIOMAN_APP_FIOD		*p_app_fiod;	/* Ptr to app fiod structure */
	FIOMAN_SYS_FIOD		*p_sys_fiod;	/* Ptr to FIOMAN fiod structure */
	FIOMSG_TX_FRAME		*tx_frame;
	FIOMSG_RX_FRAME		*rx_frame;

	/* Find this APP registration */
	p_app_fiod = fioman_find_dev( p_priv, p_arg->dev_handle );
	/* See if we found the dev_handle */
	if (NULL == p_app_fiod)
		/* No, return error */
		return -EINVAL;

	if (p_arg->tx_frame > 127)
		return -EINVAL;

	/* TBD: Make sure frame is not already scheduled */
	
	p_sys_fiod = p_app_fiod->p_sys_fiod;
	
	/* Ready a generic tx frame */
	tx_frame = fioman_ready_generic_tx_frame(p_sys_fiod, p_arg->tx_frame, p_arg->payload, p_arg->count);
	if ( IS_ERR(tx_frame) )
		/* Return failure */
		return PTR_ERR(tx_frame);
		
	/* Ready corresponding response frame ??? */
	rx_frame = fioman_ready_generic_rx_frame(p_sys_fiod, (p_arg->tx_frame + 128));
	if ( IS_ERR(rx_frame) ) {
		kfree(tx_frame);
		/* Return failure */
		return PTR_ERR(rx_frame);
	}

	/* Add to TX queue */
	fiomsg_tx_add_frame(FIOMSG_P_PORT(p_sys_fiod->fiod.port), tx_frame);
	/* Add to RX queue */
	fiomsg_rx_add_frame(FIOMSG_P_PORT(p_sys_fiod->fiod.port), rx_frame );
		
	/* Show success */
	return (0);
}

/*****************************************************************************/
/*
This function is used to read response frame size.
*/
/*****************************************************************************/

int
fioman_fiod_frame_size
(
	struct file		*filp,	/* File Pointer */
	FIO_IOC_FIOD_FRAME_SIZE	*p_arg	/* Arguments to process */
)
{
	FIOMAN_PRIV_DATA	*p_priv = filp->private_data;	/* Access Apps data */
	FIOMAN_APP_FIOD		*p_app_fiod;	/* Ptr to app fiod structure */
	FIOMSG_PORT		*p_port;
	struct list_head	*p_elem;	/* Ptr to list element */
	struct list_head	*p_next;	/* Temp for safe loop */
	FIOMSG_RX_FRAME		*p_rx_frame;	/* Ptr to rx frame being examined */

	/* Find this APP registration */
	p_app_fiod = fioman_find_dev( p_priv, p_arg->dev_handle );
	/* See if we found the dev_handle */
	if (NULL == p_app_fiod) {
		/* No, return error */
		return ( -EINVAL );
	}

	/* For each element in the list */
	p_port = FIOMSG_P_PORT( p_app_fiod->fiod.port );
	list_for_each_safe( p_elem, p_next, &p_port->rx_fiod_list[ p_app_fiod->fiod.fiod ] )
	{
		/* Get the response frame for this queue element */
		p_rx_frame = list_entry( p_elem, FIOMSG_RX_FRAME, elem );

		/* See if current element frame no is what we are looking for */
		if ( (FIOMSG_PAYLOAD( p_rx_frame )->frame_no == p_arg->rx_frame)
				&& (p_rx_frame->resp == true) )
		{
			/* Update other frame info in response */
			if (p_rx_frame->len >= 3) {
				if (NULL != p_arg->seq_number)
					put_user(p_rx_frame->info.last_seq, p_arg->seq_number);
				return (p_rx_frame->len - 2);
			}
		}
	}

	/* Show no such frame found */
	return (0);
}

/*****************************************************************************/

/*****************************************************************************/
/*
This function is used to register a frame notification.
*/
/*****************************************************************************/

int
fioman_fiod_frame_notify_register
(
	struct file			*filp,	/* File Pointer */
	FIO_IOC_FIOD_FRAME_NOTIFY_REG	*p_arg	/* Arguments to process */
)
{
	FIOMAN_PRIV_DATA	*p_priv = filp->private_data;	/* Access Apps data */
	FIOMAN_APP_FIOD		*p_app_fiod;	/* Ptr to app fiod structure */
	FIOMSG_PORT		*p_port;
	struct list_head	*p_elem;	/* Ptr to list element */
	struct list_head	*p_next;	/* Temp for safe loop */
	FIOMSG_TX_FRAME		*p_tx_frame;	/* Ptr to tx frame being examined */
	FIOMSG_RX_FRAME		*p_rx_frame;	/* Ptr to rx frame being examined */

	/* Find this APP registration */
	p_app_fiod = fioman_find_dev( p_priv, p_arg->dev_handle );
	/* See if we found the dev_handle */
	if (NULL == p_app_fiod) {
		/* No, return error */
		return (-EINVAL);
	}

	p_port = FIOMSG_P_PORT( p_app_fiod->fiod.port );

        if (p_arg->rx_frame < 128) {
                list_for_each_safe( p_elem, p_next, &p_port->tx_queue ) {
                        /* Get the tx request frame for this queue element */
                        p_tx_frame = list_entry( p_elem, FIOMSG_TX_FRAME, elem );
                        /* See if current element frame no is what we are looking for */
                        if (FIOMSG_PAYLOAD( p_tx_frame )->frame_no == p_arg->rx_frame) {
				/* indicate if repeated signal required */
				if (p_arg->notify == FIO_NOTIFY_ALWAYS)
					FIO_BIT_SET(p_app_fiod->frame_notify_type, p_arg->rx_frame);
                                /* add signal request to queue */
                                f_setown(filp, current->pid, 1);
                                filp->f_owner.signum = FIO_SIGIO;
                                if ((fasync_helper(0, filp, 1, &p_tx_frame->notify_async_queue)) >= 0) {
                                        return 0;
                                }
                        }
                }
        } else if (p_arg->rx_frame < 256) { 
                list_for_each_safe( p_elem, p_next, &p_port->rx_fiod_list[ p_app_fiod->fiod.fiod ] ) {
                        /* Get the response frame for this queue element */
                        p_rx_frame = list_entry( p_elem, FIOMSG_RX_FRAME, elem );
                        /* See if current element frame no is what we are looking for */
                        if (FIOMSG_PAYLOAD( p_rx_frame )->frame_no == p_arg->rx_frame) {
				/* indicate if repeated signal required */
				if (p_arg->notify == FIO_NOTIFY_ALWAYS)
					FIO_BIT_SET(p_app_fiod->frame_notify_type, p_arg->rx_frame);
                                /* add signal request to queue */
                                f_setown(filp, current->pid, 1);
                                filp->f_owner.signum = FIO_SIGIO;
                                if ((fasync_helper(0, filp, 1, &p_rx_frame->notify_async_queue)) >= 0) {
                                        return 0;
                                }
                        }
                }
	}

	/* Show no such frame found or system error */
	return (-EINVAL);
}

/*****************************************************************************/
/*
This function is used to deregister a frame notification.
*/
/*****************************************************************************/

int
fioman_fiod_frame_notify_deregister
(
	struct file			*filp,	/* File Pointer */
	FIO_IOC_FIOD_FRAME_NOTIFY_DEREG	*p_arg	/* Arguments to process */
)
{
	FIOMAN_PRIV_DATA	*p_priv = filp->private_data;	/* Access Apps data */
	FIOMAN_APP_FIOD		*p_app_fiod;	/* Ptr to app fiod structure */
	FIOMSG_PORT		*p_port;
	struct list_head	*p_elem;	/* Ptr to list element */
	struct list_head	*p_next;	/* Temp for safe loop */
	FIOMSG_TX_FRAME		*p_tx_frame;	/* Ptr to tx frame being examined */
	FIOMSG_RX_FRAME		*p_rx_frame;	/* Ptr to rx frame being examined */

	/* Find this APP registration */
	p_app_fiod = fioman_find_dev( p_priv, p_arg->dev_handle );
	/* See if we found the dev_handle */
	if (NULL == p_app_fiod) {
		/* No, return error */
		return (-EINVAL);
	}

	p_port = FIOMSG_P_PORT( p_app_fiod->fiod.port );

        if (p_arg->rx_frame < 128) {
                list_for_each_safe( p_elem, p_next, &p_port->tx_queue ) {
                        /* Get the tx request frame for this queue element */
                        p_tx_frame = list_entry( p_elem, FIOMSG_TX_FRAME, elem );
                        /* See if current element frame no is what we are looking for */
                        if (FIOMSG_PAYLOAD( p_tx_frame )->frame_no == p_arg->rx_frame) {
                                /* remove signal request from queue */
                                fasync_helper(0, filp, 0, &p_tx_frame->notify_async_queue);
                                FIO_BIT_CLEAR(p_app_fiod->frame_notify_type, p_arg->rx_frame);
                                return 0;
                        }
                }
        } else if (p_arg->rx_frame < 256) { 
                list_for_each_safe( p_elem, p_next, &p_port->rx_fiod_list[ p_app_fiod->fiod.fiod ] ) {
                        /* Get the response frame for this queue element */
                        p_rx_frame = list_entry( p_elem, FIOMSG_RX_FRAME, elem );
                        /* See if current element frame no is what we are looking for */
                        if (FIOMSG_PAYLOAD( p_rx_frame )->frame_no == p_arg->rx_frame) {
                                /* remove signal request from queue */
                                fasync_helper(0, filp, 0, &p_rx_frame->notify_async_queue);
                                FIO_BIT_CLEAR(p_app_fiod->frame_notify_type, p_arg->rx_frame);
                                return 0;
                        }
                }
	}

	/* Show no such frame found or system error */
	return (-EINVAL);
}

/*****************************************************************************/
/*
This function is used to query a notification.
*/
/*****************************************************************************/

int
fioman_query_frame_notify_status
(
	struct file               *filp, /* File Pointer */
	FIO_IOC_QUERY_NOTIFY_INFO *p_arg /* Arguments to process */
)
{
	FIOMAN_PRIV_DATA *p_priv = filp->private_data; /* Access Apps data */
        FIO_NOTIFY_INFO info;

        /* Is this app registered to notify? */
        if ((FIOMAN_FIFO_GET(p_priv->frame_notification_fifo, &info, sizeof(FIO_NOTIFY_INFO)) != sizeof(FIO_NOTIFY_INFO))
                || copy_to_user(p_arg->notify_info, &info, sizeof(FIO_NOTIFY_INFO)))
                        return -1;
	return 0;
}

/*****************************************************************************/

int fioman_ts1_volt_monitor_set
(
	struct file		*filp,
	FIO_IOC_TS1_VM_SET	*p_arg
)
{
	FIOMAN_PRIV_DATA	*p_priv = filp->private_data;	/* Access Apps data */
	FIOMAN_APP_FIOD		*p_app_fiod;	/* Ptr to app fiod structure */
	FIOMAN_SYS_FIOD		*p_sys_fiod;	/* Ptr to FIOMAN fiod structure */

	/* Find this APP registration */
	p_app_fiod = fioman_find_dev( p_priv, p_arg->dev_handle );
	/* See if we found the dev_handle */
	if ( NULL == p_app_fiod ) {
		/* No, return error */
		return ( -EINVAL );
	}

	if (p_app_fiod->fiod.fiod != FIOTS1) {
		return (-EINVAL);
	}

	p_sys_fiod = p_app_fiod->p_sys_fiod;
	if (p_arg->vms == FIO_TS1_VM_ON)
		p_sys_fiod->vm_state = p_app_fiod->vm_state = FIO_TS1_VM_ON;
	else if (p_arg->vms == FIO_TS1_VM_OFF)
		p_sys_fiod->vm_state = p_app_fiod->vm_state = FIO_TS1_VM_OFF;
	else
		return (-EINVAL);

	return 0;
}


int fioman_ts1_volt_monitor_get
(
	struct file		*filp,
	FIO_IOC_TS1_VM_GET	*p_arg
)
{
	FIOMAN_PRIV_DATA	*p_priv = filp->private_data;	/* Access Apps data */
	FIOMAN_APP_FIOD		*p_app_fiod;	/* Ptr to app fiod structure */
	FIOMAN_SYS_FIOD		*p_sys_fiod;	/* Ptr to FIOMAN fiod structure */

	/* Find this APP registration */
	p_app_fiod = fioman_find_dev( p_priv, p_arg->dev_handle );
	/* See if we found the dev_handle */
	if ( NULL == p_app_fiod ) {
		/* No, return error */
		return ( -EINVAL );
	}

	if (p_app_fiod->fiod.fiod != FIOTS1) {
		return (-EINVAL);
	}
	if (p_arg->vms == NULL)
		return(-EINVAL);

	/* Is system view or app view requested? */
	if (p_arg->view == FIO_VIEW_SYSTEM) {
		p_sys_fiod = p_app_fiod->p_sys_fiod;
		put_user(p_sys_fiod->vm_state, p_arg->vms);
	} else if (p_arg->view == FIO_VIEW_APP) {
		put_user(p_app_fiod->vm_state, p_arg->vms);
	} else {
		/* error, invalid view specified */
		return(-EINVAL);
	}

	return 0;
}
#ifdef TS2_PORT1_STATE
int fioman_ts2_port1_state
(
	struct file		*filp,		/* File Pointer */
	FIO_IOC_TS2_PORT1_STATE	*p_arg
)
{
	FIOMAN_PRIV_DATA	*p_priv = filp->private_data;	/* Access Apps data */
        FIO_PORT                port = p_arg->port;
        FIOMSG_PORT             *p_port;
        int status;
        int retval = -1;
        
        /* Make sure serial port is opened */
        pr_debug("fioman_ts2_port1_state: check if port %d is open\n", port); 
	if (p_port->port_opened) {
                pr_debug("fioman_ts2_port1_state: port %d is open\n", port); 
		p_port = &fio_port_table[port];
                /* get modem pin status from port */
                if ((retval = sdlc_kernel_ioctl(p_port->context, TIOCMGET, (unsigned long)&status)) == 0) {
                        pr_debug("fioman_ts2_port1_state: port %d status=%x\n", port, status); 
                        /* check DCD status for port1 disable state */
                        if (status & TIOCM_CAR)
                                put_user(FIO_TS2_PORT1_ENABLED, p_arg->state);
                        else
                                put_user(FIO_TS2_PORT1_DISABLED, p_arg->state);
                }
	}

	return retval;
}
#endif
int fioman_frame_schedule_set
(
	struct file			*filp,
	FIO_IOC_FIOD_FRAME_SCHD_SET	*p_arg
)
{
	FIOMAN_PRIV_DATA	*p_priv = filp->private_data;	/* Access Apps data */
	FIOMAN_APP_FIOD		*p_app_fiod;	/* Ptr to app fiod structure */
	FIOMAN_SYS_FIOD		*p_sys_fiod;	/* Ptr to FIOMAN fiod structure */
	struct list_head	*p_elem;	/* Ptr to queue element being examined */
	struct list_head	*p_next;	/* Temp Ptr to next for loop */
	struct list_head	*p_app_elem;	/* Ptr to app element being examined */
	FIOMSG_TX_FRAME		*p_tx_elem;	/* Ptr to tx frame being examined */
	FIOMSG_PORT		*p_port;	/* Port of Request Queue */
	FIO_FRAME_SCHD		frame_schd[128] = {{0},{0}};
	int			i;
        bool                    resched;
	
	/* Find this APP registration */
	p_app_fiod = fioman_find_dev( p_priv, p_arg->dev_handle );
	/* See if we found the dev_handle */
	if ( NULL == p_app_fiod ) {
		/* No, return error */
		return ( -EINVAL );
	}

	if ( (p_arg->count > 127) || copy_from_user(frame_schd,
		p_arg->frame_schd, p_arg->count*sizeof(FIO_FRAME_SCHD)) ) {
		return ( -EFAULT );
	}

	p_sys_fiod = p_app_fiod->p_sys_fiod;

	/* VALIDATE array for valid frame number and frequency !!!*/
	for (i=0; i<p_arg->count; i++) {
		if ((frame_schd[i].req_frame > 127)
			|| (frame_schd[i].frequency >= FIO_HZ_MAX)
			|| (p_sys_fiod->frame_frequency_table[frame_schd[i].req_frame] < FIO_HZ_0)) {
pr_debug("fioman_frame_schedule_set: item:%d frame:%d freq:%d(%d) invalid\n",
        i, frame_schd[i].req_frame, frame_schd[i].frequency, p_sys_fiod->frame_frequency_table[frame_schd[i].req_frame]);
			return( -EINVAL );
                }
	}

	/* Update this app's frame frequency settings, and system settings if required */
	for (i=0; i<p_arg->count; i++) {
		unsigned int frame = frame_schd[i].req_frame;
		FIO_HZ new_freq = frame_schd[i].frequency;
		p_app_fiod->frame_frequency_table[frame] = new_freq;
		if ((p_sys_fiod->frame_frequency_table[frame] != new_freq) || (new_freq == FIO_HZ_ONCE)) {
			/* Set current frame frequency to highest setting of all apps */
			p_sys_fiod->frame_frequency_table[frame] = new_freq;
			list_for_each( p_app_elem, &p_sys_fiod->app_fiod_list ) {
				/* Get a ptr to this list entry */
				p_app_fiod = list_entry( p_app_elem, FIOMAN_APP_FIOD, sys_elem );
				if (p_app_fiod->frame_frequency_table[frame] > p_sys_fiod->frame_frequency_table[frame])
					p_sys_fiod->frame_frequency_table[frame] = p_app_fiod->frame_frequency_table[frame];
					
			}
			/* Update the frame in tx queue */
			/* Get port to work on */
			p_port = FIOMSG_P_PORT( p_sys_fiod->fiod.port );
                        resched = false;
                        new_freq = p_sys_fiod->frame_frequency_table[frame];
			/* For each element in the queue */
			list_for_each_safe( p_elem, p_next, &p_port->tx_queue ) {
				/* Get the request frame for this queue element */
				p_tx_elem = list_entry( p_elem, FIOMSG_TX_FRAME, elem );
				/* See if current element is destined for FIOD interested in */
				if ( p_tx_elem->fiod.fiod == p_sys_fiod->fiod.fiod ) {
					/* Does the frame number match one requested? */
					if (FIOMSG_PAYLOAD( p_tx_elem )->frame_no == frame) {
pr_debug("fioman_frame_schedule_set: updating queued frame:%d freq:%d\n", frame, new_freq);
                                                resched = true;
                                                if (new_freq == FIO_HZ_0) {
                                                        /* remove this element */
                                                        list_del_init( p_elem );
                                                        /* Free memory with this TX buffer */
                                                        kfree( p_tx_elem );
                                                } else {
                                                        p_tx_elem->cur_freq = new_freq;
                                                }
					}
				}
			}
			/* if the frame wasn't found in the tx queue, schedule it? */
                        if (!resched) {
pr_debug("fioman_frame_schedule_set: frame:%d not currently scheduled\n", frame);
                                /* Ready frame for this device */
                                if (fioman_add_frame(frame, p_sys_fiod) != 0)
                                        return -EFAULT;
                        }
		}
	}

	return 0;
}

int fioman_frame_schedule_get
(
	struct file			*filp,
	FIO_IOC_FIOD_FRAME_SCHD_GET	*p_arg
)
{
	FIOMAN_PRIV_DATA	*p_priv = filp->private_data;	/* Access Apps data */
	FIOMAN_APP_FIOD		*p_app_fiod;	/* Ptr to app fiod structure */
	FIOMAN_SYS_FIOD		*p_sys_fiod;	/* Ptr to FIOMAN fiod structure */
	FIO_FRAME_SCHD		frame_schd[128] = {{0},{0}};
	int			i;
	
	/* Find this APP registration */
	p_app_fiod = fioman_find_dev( p_priv, p_arg->dev_handle );
	/* See if we found the dev_handle */
	if ( NULL == p_app_fiod ) {
		/* No, return error */
		return ( -EINVAL );
	}

	if ((p_arg->count > 127) || copy_from_user(frame_schd, p_arg->frame_schd,
		(p_arg->count * sizeof(FIO_FRAME_SCHD)))) {
		return ( -EFAULT );
	}

	/* VALIDATE array for valid frame number!!!*/
	for (i=0; i<p_arg->count; i++) {
		if (frame_schd[i].req_frame > 127) {
pr_debug("fioman_frame_schedule_get: item:%d frame:%d invalid\n", i,
		frame_schd[i].req_frame);
			return( -EINVAL );
                }
	}
	/* Is system view or app view requested? */
	if (p_arg->view == FIO_VIEW_SYSTEM) {
		p_sys_fiod = p_app_fiod->p_sys_fiod;
		for (i=0; i<p_arg->count; i++) {
			int freq = p_sys_fiod->frame_frequency_table[frame_schd[i].req_frame];
			if (freq < FIO_HZ_0)
				frame_schd[i].frequency = FIO_HZ_0;
			else
				frame_schd[i].frequency = freq;
		}
	} else if (p_arg->view == FIO_VIEW_APP) {
		for (i=0; i<p_arg->count; i++) {
			frame_schd[i].frequency
				= p_app_fiod->frame_frequency_table[frame_schd[i].req_frame];
		} 
	} else {
		/* error, invalid view specified */
		return(-EINVAL);
	}

	/* Copy filled in array back to user */
	if (copy_to_user(p_arg->frame_schd, frame_schd, (p_arg->count * sizeof(FIO_FRAME_SCHD)))) {
		/* Could not copy for some reason */
		return ( -EFAULT );
	}

	return 0;
}

int fioman_inputs_filter_set
(
	struct file			*filp,
	FIO_IOC_FIOD_INPUTS_FILTER_SET	*p_arg
)
{
	FIOMAN_PRIV_DATA	*p_priv = filp->private_data;	/* Access Apps data */
	FIOMAN_APP_FIOD		*p_app_fiod;	/* Ptr to app fiod structure */
	FIOMAN_SYS_FIOD		*p_sys_fiod;	/* Ptr to FIOMAN fiod structure */
	FIO_INPUT_FILTER	filter;
	int			i;
	bool update_fiod = false;
	unsigned long flags;
	
	/* Find this APP registration */
	p_app_fiod = fioman_find_dev( p_priv, p_arg->dev_handle );
	/* See if we found the dev_handle */
	if ( NULL == p_app_fiod )
		/* No, return error */
		return -EINVAL;
	
	if (p_arg->count >= (FIO_INPUT_POINTS_BYTES*8))
		return ( -EFAULT );

	/* VALIDATE array for valid input number and filter values */
	for (i=0; i<p_arg->count; i++) {
		if ((copy_from_user(&filter, &p_arg->input_filter[i], sizeof(FIO_INPUT_FILTER)))
			|| (filter.input_point >= (FIO_INPUT_POINTS_BYTES * 8))
			|| (filter.leading > 255) || (filter.trailing > 255)) {
pr_debug("fioman_inputs_filter_set: item:%d ip:%d lead:%d trail:%d invalid\n",
        i, filter.input_point, filter.leading, filter.trailing);
			return( -EINVAL );
                }
	}

	p_sys_fiod = p_app_fiod->p_sys_fiod;
	spin_lock_irqsave(&p_sys_fiod->lock, flags);
	for (i=0; i<p_arg->count; i++) {
		__copy_from_user(&filter, &p_arg->input_filter[i], sizeof(FIO_INPUT_FILTER));
		/* Save app-based values */
		p_app_fiod->input_filters_leading[filter.input_point] = filter.leading;
		p_app_fiod->input_filters_trailing[filter.input_point] = filter.trailing;
		/* Use lowest filter values of all apps */
		if (filter.leading < p_sys_fiod->input_filters_leading[filter.input_point]) {
			p_sys_fiod->input_filters_leading[filter.input_point] = filter.leading;
			update_fiod = true;
		} else {
			filter.leading = p_sys_fiod->input_filters_leading[filter.input_point];
		}
		if (filter.trailing < p_sys_fiod->input_filters_trailing[filter.input_point]) {
			p_sys_fiod->input_filters_trailing[filter.input_point] = filter.trailing;
			update_fiod = true;
		} else {
			filter.trailing = p_sys_fiod->input_filters_trailing[filter.input_point];
		}
		/* Return system view values to user */
		copy_to_user(&p_arg->input_filter[i], &filter, sizeof(FIO_INPUT_FILTER));
	}
	spin_unlock_irqrestore(&p_sys_fiod->lock, flags);

	/* If any sys_fiod filter values have changed, we must schedule frame #51 */
	if (update_fiod) {
		/* Ready frame 51 for this device */
                if (!fioman_frame_is_scheduled(p_sys_fiod, FIOMAN_FRAME_NO_51))
                        return fioman_add_frame(FIOMAN_FRAME_NO_51, p_sys_fiod);
	}
	
	return 0;
}

int fioman_inputs_filter_get
(
	struct file			*filp,
	FIO_IOC_FIOD_INPUTS_FILTER_GET	*p_arg
)
{
	FIOMAN_PRIV_DATA	*p_priv = filp->private_data;	/* Access Apps data */
	FIOMAN_APP_FIOD		*p_app_fiod;	/* Ptr to app fiod structure */
	FIOMAN_SYS_FIOD		*p_sys_fiod;	/* Ptr to FIOMAN fiod structure */
	FIO_INPUT_FILTER	filter;
	int			i;

	/* Find this APP registration */
	p_app_fiod = fioman_find_dev( p_priv, p_arg->dev_handle );
	/* See if we found the dev_handle */
	if ( NULL == p_app_fiod )
		/* No, return error */
		return -EINVAL;
	
	/* Is system view or app view requested? (Does APP VIEW make sense?) */
	if ((p_arg->view != FIO_VIEW_SYSTEM) && (p_arg->view != FIO_VIEW_APP))
		/* error, invalid view specified */
		return -EINVAL;

	if (p_arg->count >= FIO_INPUT_POINTS_BYTES)
		return -EINVAL;

	/* VALIDATE array for valid input number */
	for (i=0; i<p_arg->count; i++) {
		if (p_arg->input_filter[i].input_point >= (FIO_INPUT_POINTS_BYTES*8))
			return -EINVAL;
	}

	p_sys_fiod = p_app_fiod->p_sys_fiod;
	for (i=0; i<p_arg->count; i++) {
		copy_from_user(&filter, &p_arg->input_filter[i], sizeof(FIO_INPUT_FILTER));
		if (p_arg->view == FIO_VIEW_SYSTEM) {
			filter.leading = p_sys_fiod->input_filters_leading[filter.input_point];
			filter.trailing = p_sys_fiod->input_filters_trailing[filter.input_point];
		} else {
			filter.leading = p_app_fiod->input_filters_leading[filter.input_point];
			filter.trailing = p_app_fiod->input_filters_trailing[filter.input_point];
		}
		copy_to_user(&p_arg->input_filter[i], &filter, sizeof(FIO_INPUT_FILTER));
	}
	
	return 0;
}

int fioman_inputs_trans_set
(
	struct file			*filp,
	FIO_IOC_INPUTS_TRANS_SET	*p_arg
)
{
	FIOMAN_PRIV_DATA	*p_priv = filp->private_data;	/* Access Apps data */
	FIOMAN_APP_FIOD		*p_app_fiod;	/* Ptr to app fiod structure */
	FIOMAN_SYS_FIOD		*p_sys_fiod;	/* Ptr to FIOMAN fiod structure */
	unsigned char		input_trans_map[FIO_INPUT_POINTS_BYTES];
	int i, count;
	unsigned long flags;
	bool update_fiod = false;

	/* Find this APP registration */
	p_app_fiod = fioman_find_dev( p_priv, p_arg->dev_handle );
	/* See if we found the dev_handle */
	if ( NULL == p_app_fiod )
		/* No, return error */
		return -EINVAL;
	
        count = p_arg->count;
	if (count <= 0) {
		return ( -EFAULT );
	}

	if (count > FIO_INPUT_POINTS_BYTES)
                count = FIO_INPUT_POINTS_BYTES;

	p_sys_fiod = p_app_fiod->p_sys_fiod;
	if (copy_from_user(input_trans_map, p_arg->data, count)) {
		return -EFAULT;
	}

	spin_lock_irqsave(&p_sys_fiod->lock, flags);
	for (i=0; i<(FIO_INPUT_POINTS_BYTES*8); i++) {
		/* Save app-based values */
		if (FIO_BIT_TEST(input_trans_map,i)) {
			FIO_BIT_SET(p_app_fiod->input_transition_map, i);
			if (!FIO_BIT_TEST(p_sys_fiod->input_transition_map,i)) {
				FIO_BIT_SET(p_sys_fiod->input_transition_map, i);
				update_fiod = true;
			}
		} else {
			FIO_BIT_CLEAR(p_app_fiod->input_transition_map, i);
			if (FIO_BIT_TEST(p_sys_fiod->input_transition_map, i)) {
				/* TBD: Check all apps before turning off */
			}
		}
	}
	spin_unlock_irqrestore(&p_sys_fiod->lock, flags);

	/* If any sys_fiod input config values have changed, we must schedule frame #51 */
	if (update_fiod) {
		/* Ready frame 51 for this device */
                if (!fioman_frame_is_scheduled(p_sys_fiod, FIOMAN_FRAME_NO_51)) 
                        return fioman_add_frame(FIOMAN_FRAME_NO_51, p_sys_fiod);
	}
	
	return 0;
}

int fioman_inputs_trans_get
(
	struct file			*filp,
	FIO_IOC_INPUTS_TRANS_GET	*p_arg
)
{
	FIOMAN_PRIV_DATA	*p_priv = filp->private_data;	/* Access Apps data */
	FIOMAN_APP_FIOD		*p_app_fiod;	/* Ptr to app fiod structure */
	FIOMAN_SYS_FIOD		*p_sys_fiod;	/* Ptr to FIOMAN fiod structure */

	/* Find this APP registration */
	p_app_fiod = fioman_find_dev( p_priv, p_arg->dev_handle );
	/* See if we found the dev_handle */
	if ( NULL == p_app_fiod ) {
		/* No, return error */
		return -EINVAL;
	}
	
	if (p_arg->count > FIO_INPUT_POINTS_BYTES) {
		return -EINVAL;
	}

	p_sys_fiod = p_app_fiod->p_sys_fiod;
	switch (p_arg->view) {
	case FIO_VIEW_APP:
		if (copy_to_user(p_arg->data, p_app_fiod->input_transition_map, p_arg->count))
			return -EFAULT;
		break;
	case FIO_VIEW_SYSTEM:
		if (copy_to_user(p_arg->data, p_sys_fiod->input_transition_map, p_arg->count))
			return -EFAULT;
		break;
	default:
		/* Invalid View */
		return -EINVAL;
		break;
	}
	
	return 0;
}

int fioman_inputs_trans_read
(
	struct file			*filp,
	FIO_IOC_INPUTS_TRANS_READ	*p_arg
)
{
	FIOMAN_PRIV_DATA	*p_priv = filp->private_data;	/* Access Apps data */
	FIOMAN_APP_FIOD		*p_app_fiod;	/* Ptr to app fiod structure */
	FIOMAN_SYS_FIOD		*p_sys_fiod;	/* Ptr to FIOMAN fiod structure */
        FIO_TRANS_STATUS        status;
        FIO_TRANS_BUFFER        buffer;
	int			i = 0, count = 0;
        unsigned long flags;
	
	/* Find this APP registration */
	p_app_fiod = fioman_find_dev( p_priv, p_arg->dev_handle );
	/* See if we found the dev_handle */
	if ( NULL == p_app_fiod )
		/* No, return error */
		return -EINVAL;
	
	if ((p_arg->status == NULL) || (p_arg->trans_buf == NULL) || (p_arg->count <= 0))
		return -EINVAL;

	p_sys_fiod = p_app_fiod->p_sys_fiod;

	/* TBD: return app-specific transition buffer entries */
	spin_lock_irqsave(&p_sys_fiod->lock, flags);
	count = FIOMAN_FIFO_LEN(p_app_fiod->transition_fifo)/sizeof(FIO_TRANS_BUFFER);
        status = p_app_fiod->transition_status;
        /* Reset status so reported only once per occurrence */
        p_app_fiod->transition_status = FIO_TRANS_SUCCESS;
	spin_unlock_irqrestore(&p_sys_fiod->lock, flags);
	if (p_arg->count < count)
		count = p_arg->count;
pr_debug("fioman_inputs_trans_read: reading %d entries from fifo@%p len=%d\n", count, &p_app_fiod->transition_fifo, FIOMAN_FIFO_LEN(p_app_fiod->transition_fifo));
	for (i=0; i<count; i++) {
                if ((FIOMAN_FIFO_GET(p_app_fiod->transition_fifo, &buffer, sizeof(FIO_TRANS_BUFFER)) != sizeof(FIO_TRANS_BUFFER))
                        || copy_to_user(&p_arg->trans_buf[i], &buffer, sizeof(FIO_TRANS_BUFFER)))
                        return -1;
	}
        put_user(status, p_arg->status);

	return count;
}

/*****************************************************************************/
/*
This function is used to register for watchdog service.
*/
/*****************************************************************************/

int
fioman_wd_register
(
	struct file		*filp,		/* File Pointer */
	FIO_DEV_HANDLE		dev_handle	/* Previously returned dev_handle */
)
{
	FIOMAN_PRIV_DATA	*p_priv = filp->private_data;	/* Access Apps data */
	FIOMAN_APP_FIOD		*p_app_fiod;	/* Ptr to app fiod structure */

	/* Find this APP registration */
	p_app_fiod = fioman_find_dev( p_priv, dev_handle );
	/* See if we found the dev_handle */
	if ( NULL == p_app_fiod ) {
		/* No, return error */
		return ( -EINVAL );
	}

	p_app_fiod->watchdog_reservation = true;
	return 0;
}

/*****************************************************************************/
/*
This function is used to deregister from watchdog service.
*/
/*****************************************************************************/

int
fioman_wd_deregister
(
	struct file		*filp,		/* File Pointer */
	FIO_DEV_HANDLE		dev_handle	/* Previously returned dev_handle */
)
{
	FIOMAN_PRIV_DATA	*p_priv = filp->private_data;	/* Access Apps data */
	FIOMAN_APP_FIOD		*p_app_fiod;	/* Ptr to app fiod structure */

	/* Find this APP registration */
	p_app_fiod = fioman_find_dev( p_priv, dev_handle );
	/* See if we found the dev_handle */
	if ( NULL == p_app_fiod ) {
		/* No, return error */
		return ( -EINVAL );
	}

	p_app_fiod->watchdog_reservation = false;
#ifdef NEW_WATCHDOG
        p_app_fiod->watchdog_rate = FIO_HZ_0;
#endif
	return 0;
}

int fioman_wd_reservation_set
(
	struct file		*filp,
	FIO_IOC_FIOD_WD_RES_SET	*p_arg
)
{
	FIOMAN_PRIV_DATA	*p_priv = filp->private_data;	/* Access Apps data */
	FIOMAN_APP_FIOD		*p_app_fiod;	/* Ptr to app fiod structure */
	FIOMAN_SYS_FIOD		*p_sys_fiod;	/* Ptr to FIOMAN fiod structure */

	/* Find this APP registration */
	p_app_fiod = fioman_find_dev( p_priv, p_arg->dev_handle );
	/* See if we found the dev_handle */
	if ( NULL == p_app_fiod ) {
		/* No, return error */
		return -EINVAL;
	}

	/* app not registered for watchdog service */
	if (p_app_fiod->watchdog_reservation == false)
		return -EACCES;

	/* invalid arg or output number out-of-range */
	if ((p_arg == NULL) || (p_arg->output < 0) || (p_arg->output > 103))
		return -EINVAL;
	
	p_sys_fiod = p_app_fiod->p_sys_fiod;

	/* watchdog output already established */
	if (p_sys_fiod->watchdog_output != -1) {
		return -EEXIST;
	}

	/* output point reserved as regular output? */
	if (FIO_BIT_TEST(p_sys_fiod->outputs_reserved, p_arg->output))
		return -EACCES;
	
	p_sys_fiod->watchdog_output = p_arg->output;
	
	return 0;
}


int fioman_wd_reservation_get
(
	struct file		*filp,
	FIO_IOC_FIOD_WD_RES_GET	*p_arg
)
{
	FIOMAN_PRIV_DATA	*p_priv = filp->private_data;	/* Access Apps data */
	FIOMAN_APP_FIOD		*p_app_fiod;	/* Ptr to app fiod structure */
	FIOMAN_SYS_FIOD		*p_sys_fiod;	/* Ptr to FIOMAN fiod structure */

	/* Find this APP registration */
	p_app_fiod = fioman_find_dev( p_priv, p_arg->dev_handle );
	/* See if we found the dev_handle */
	if ((p_app_fiod == NULL) || (p_arg == NULL)) {
		/* No, return error */
		return -EINVAL;
	}

	p_sys_fiod = p_app_fiod->p_sys_fiod;
	if (p_sys_fiod->watchdog_output < 0)
		return -ENODEV;
		
	return put_user(p_sys_fiod->watchdog_output, p_arg->output);
}

#ifdef NEW_WATCHDOG
int fioman_wd_rate_set
(
	struct file		        *filp,
	FIO_IOC_FIOD_WD_RATE_SET        *p_arg
)
{
	FIOMAN_PRIV_DATA	*p_priv = filp->private_data;	/* Access Apps data */
	FIOMAN_APP_FIOD		*p_app_fiod;	/* Ptr to app fiod structure */
	FIOMAN_SYS_FIOD		*p_sys_fiod;	/* Ptr to FIOMAN fiod structure */
	struct list_head	*p_app_elem;	/* Ptr to app element being examined */
        FIO_HZ                  new_rate;

	/* Find this APP registration */
	p_app_fiod = fioman_find_dev( p_priv, p_arg->dev_handle );
	/* See if we found the dev_handle */
	if ( NULL == p_app_fiod ) {
		/* No, return error */
		return -EINVAL;
	}

	/* app not registered for watchdog or health monitor service */
	if ((p_app_fiod->watchdog_reservation == false) || (p_priv->hm_timeout == 0))
		return -EACCES;

	/* invalid arg or output number out-of-range */
	if ((p_arg == NULL) || (p_arg->rate < FIO_HZ_0) || (p_arg->rate >= FIO_HZ_MAX))
		return -EINVAL;
	
        p_app_fiod->watchdog_rate = new_rate = p_arg->rate;
        
	p_sys_fiod = p_app_fiod->p_sys_fiod;

        /* Check for new highest wd rate of all apps */
	p_app_fiod = NULL;
	list_for_each( p_app_elem, &p_sys_fiod->app_fiod_list )
	{
		/* Get a ptr to this list entry */
		p_app_fiod = list_entry( p_app_elem, FIOMAN_APP_FIOD, sys_elem );
		if (p_app_fiod->watchdog_reservation) {
                        if (p_app_fiod->watchdog_rate > new_rate) {
                                new_rate = p_app_fiod->watchdog_rate;
                        }
                }
	}

	if (new_rate > p_sys_fiod->watchdog_rate) {
                p_sys_fiod->watchdog_rate = new_rate;
                pr_debug("fioman_wd_rate_set: new rate %d\n", new_rate);
        }
	
	return 0;
}
#endif
/*****************************************************************************/
/*
This function is used to request toggle of the watchdog output state.
*/
/*****************************************************************************/

int
fioman_wd_heartbeat
(
	FIOMAN_PRIV_DATA	*p_priv,	/* Apps private data */
	FIO_DEV_HANDLE		dev_handle	/* Previously returned dev_handle */
)
{
	FIOMAN_APP_FIOD		*p_app_fiod;	/* Ptr to app fiod structure */
	FIOMAN_SYS_FIOD		*p_sys_fiod;	/* Ptr to FIOMAN fiod structure */
	struct list_head	*p_app_elem;	/* Ptr to app element being examined */
	unsigned long		flags;

	/* Find this APP registration */
	p_app_fiod = fioman_find_dev( p_priv, dev_handle );
	/* See if we found the dev_handle */
	if ( NULL == p_app_fiod ) {
		/* No, return error */
		return ( -EINVAL );
	}

	if (p_app_fiod->watchdog_reservation == false)
		return -EACCES;
		
	p_sys_fiod = p_app_fiod->p_sys_fiod;
	if (p_sys_fiod->watchdog_output < 0)
		return -ENODEV;

	p_app_fiod->watchdog_toggle_pending = true;
	
#ifndef NEW_WATCHDOG
	/* Don't toggle output if set outputs frame has not yet been sent */
	if (p_sys_fiod->watchdog_trigger_condition == true)
		return 0;
#endif
	/* Check toggle pending status of all watchdog clients for this fiod */
	/* if all have toggled, change watchdog state, then clear all pending flags */
	p_app_fiod = NULL;
	list_for_each( p_app_elem, &p_sys_fiod->app_fiod_list )
	{
		/* Get a ptr to this list entry */
		p_app_fiod = list_entry( p_app_elem, FIOMAN_APP_FIOD, sys_elem );
		if (p_app_fiod->watchdog_reservation)
		{
			if (!p_app_fiod->watchdog_toggle_pending) {
				/* This app fiod has not toggled yet */
				return 0;
			}
		}
	}
	/* All apps have toggled watchdog */
	spin_lock_irqsave(&p_sys_fiod->lock, flags);
#ifndef NEW_WATCHDOG
	p_sys_fiod->watchdog_trigger_condition = true;
#endif
	p_sys_fiod->watchdog_state = !p_sys_fiod->watchdog_state;
	FIO_BIT_CLEAR(p_sys_fiod->outputs_plus, p_sys_fiod->watchdog_output);
	FIO_BIT_CLEAR(p_sys_fiod->outputs_minus, p_sys_fiod->watchdog_output);
	if (p_sys_fiod->watchdog_state) {
		FIO_BIT_SET(p_sys_fiod->outputs_plus, p_sys_fiod->watchdog_output);	
		FIO_BIT_SET(p_sys_fiod->outputs_minus, p_sys_fiod->watchdog_output);
	}
	spin_unlock_irqrestore(&p_sys_fiod->lock, flags);
pr_debug( "fioman_wd_heartbeat: plus:%x %x %x %x %x %x %x %x %x %x %x %x %x\n",
	p_sys_fiod->outputs_plus[0], p_sys_fiod->outputs_plus[1], p_sys_fiod->outputs_plus[2], p_sys_fiod->outputs_plus[3],
	p_sys_fiod->outputs_plus[4], p_sys_fiod->outputs_plus[5], p_sys_fiod->outputs_plus[6], p_sys_fiod->outputs_plus[7],
	p_sys_fiod->outputs_plus[8], p_sys_fiod->outputs_plus[9], p_sys_fiod->outputs_plus[10], p_sys_fiod->outputs_plus[11],
	p_sys_fiod->outputs_plus[12]);
	
	/* Clear all toggle pending flags */
	p_app_fiod = NULL;
	list_for_each( p_app_elem, &p_sys_fiod->app_fiod_list )
	{
		/* Get a ptr to this list entry */
		p_app_fiod = list_entry( p_app_elem, FIOMAN_APP_FIOD, sys_elem );
		if (p_app_fiod->watchdog_reservation)
		{
			p_app_fiod->watchdog_toggle_pending = false;
		}
	}
	
	return 0;
}


void hm_timeout(struct work_struct *work)
{
        FIOMAN_PRIV_DATA        *p_priv = hm_timeout_priv;
	FIOMAN_APP_FIOD		*p_app_fiod;	/* Ptr to app fiod structure */
	struct list_head 	*p_app_elem;	/* Ptr to app list element */
#ifdef LAZY_CLOSE
        struct list_head        *p_app_next;
        struct list_head        *p_priv_elem;
        struct list_head        *p_priv_next;
        FIOMAN_PRIV_DATA        *p_priv_old;
#endif

        pr_debug("hm_timeout: disable failed app @%p\n", p_priv);

        if (p_priv == NULL)
                return;
                
	list_for_each( p_app_elem, &p_priv->fiod_list )
	{
		/* Get a ptr to this list entry */
		p_app_fiod = list_entry( p_app_elem, FIOMAN_APP_FIOD, elem );
		/* See if this FIOD is currently enabled */
		if ( p_app_fiod->enabled ) {
			/* Disable the FIOD, as the APP currently has enabled */
			fioman_do_disable_fiod( p_app_fiod );
		}
                p_app_fiod->hm_disabled = true;
	}

#ifdef LAZY_CLOSE
        /* search for this app priv_data in lazy close list */
        list_for_each_safe(p_priv_elem, p_priv_next, &priv_list) {
                p_priv_old = list_entry(p_priv_elem, FIOMAN_PRIV_DATA, elem);
                if (p_priv_old->hm_fault) {
                        /* found in lazy close list and hm timeout */
                        /* Deregister every FIOD still registered */
                        list_for_each_safe( p_app_elem, p_app_next, &p_priv_old->fiod_list )
                        {
                                /* Get a ptr to this list entry */
                                p_app_fiod = list_entry( p_app_elem, FIOMAN_APP_FIOD, elem );

                                /* Do the deregister for this FIOD */
                                fioman_do_dereg_fiod( p_app_fiod );
                        }
                        /* Delete this list entry */
                        list_del_init( p_priv_elem );

                        /* Free the memory */
                        kfree( p_priv_old );
                        return;
                }
        }
#endif
        
        return;
}

/*****************************************************************************/
/*
This function is the health monitor service timeout handler.
*/
/*****************************************************************************/
void hm_timer_alarm( unsigned long arg )
{
	FIOMAN_PRIV_DATA 	*p_priv = (FIOMAN_PRIV_DATA *)arg;	/* Access Apps data */
	
	/* app hm timer has expired */
	p_priv->hm_fault = true;
	/* disable all app_fiods */
        /* can't perform from irq context, so schedule work */
        hm_timeout_priv = p_priv;
        schedule_work(&hm_timeout_work);
	
	return;
}

/*****************************************************************************/
/*
This function is used to register for health monitor service.
*/
/*****************************************************************************/

int
fioman_hm_register
(
	struct file		*filp,		/* File Pointer */
	unsigned int		timeout		/* Health Monitor timeout to use */
)
{
	FIOMAN_PRIV_DATA	*p_priv = filp->private_data;	/* Access Apps data */

	
	if (timeout) {
		/* Set new timeout alarm */
		p_priv->hm_timeout = timeout;
		if (!p_priv->hm_fault)
			/* use heartbeat function to start timer */
			fioman_hm_heartbeat(filp);
	} else {
		/* use deregister function to cancel timer */
		fioman_hm_deregister(filp);
	}	
        pr_debug("fioman_hm_register: %s app %p\n", timeout?"enabling":"disabling", p_priv);
	return 0;
}

/*****************************************************************************/
/*
This function is used to deregister from health monitor service.
*/
/*****************************************************************************/

int
fioman_hm_deregister
(
	struct file		*filp		/* File Pointer */
)
{
	FIOMAN_PRIV_DATA	*p_priv = filp->private_data;	/* Access Apps data */

	/* cancel hm timer */
        if (timer_pending(&p_priv->hm_timer))
        	del_timer(&p_priv->hm_timer);
	p_priv->hm_timeout = 0;
        pr_debug("fioman_hm_deregister: app %p\n", p_priv);

	return 0;
}

/*****************************************************************************/
/*
This function is used to refresh/heartbeat the health monitor service.
*/
/*****************************************************************************/

int
fioman_hm_heartbeat
(
	struct file		*filp		/* File Pointer */
)
{
	FIOMAN_PRIV_DATA	*p_priv = filp->private_data;	/* Access Apps data */

        if (p_priv->hm_fault)
                return 1;               /* timed out, must use hm_fault_reset() */
                
	if (p_priv->hm_timeout == 0)
                return -EACCES;        /* not registered for hb service */
                
		mod_timer(&p_priv->hm_timer,
			jiffies + msecs_to_jiffies(p_priv->hm_timeout*100));
        pr_debug("fioman_hm_heartbeat: app %p mod_timer now=%ld expire=%ld\n", p_priv, jiffies, msecs_to_jiffies(p_priv->hm_timeout*100));

	return 0;
}

/*****************************************************************************/
/*
This function is used to reset the health monitor service after a timeout.
*/
/*****************************************************************************/

int
fioman_hm_fault_reset
(
	struct file		*filp		/* File Pointer */
)
{
	FIOMAN_PRIV_DATA	*p_priv = filp->private_data;	/* Access Apps data */
	FIOMAN_APP_FIOD		*p_app_fiod;	/* Ptr to app fiod structure */
	struct list_head	*p_app_elem;	/* Ptr to app list element */

        if (p_priv->hm_timeout == 0)
                return -EACCES;        /* not registered for hm service */
                
	if (p_priv->hm_fault) {
		/* re-enable fio devices */
		p_app_fiod = NULL;
		list_for_each( p_app_elem, &p_priv->fiod_list )
		{
			/* Get a ptr to this list entry */
			p_app_fiod = list_entry( p_app_elem, FIOMAN_APP_FIOD, elem );
			if ( p_app_fiod->hm_disabled ) {
				p_app_fiod->hm_disabled = false;
				fioman_enable_fiod(filp, p_app_fiod->dev_handle);
			}
		}		
		p_priv->hm_fault = false;
		fioman_hm_heartbeat(filp);
	}
	return 0;
}

int
fioman_set_local_time_offset
(
	struct file			*filp,	/* File Pointer */
	const int			*p_arg	/* Arguments to process */
)
{
	if (get_user(local_time_offset, p_arg) != 0) {
		printk(KERN_ALERT "failed to get local_time_offset\n");
		return -1;
	} else {
		printk(KERN_ALERT "local_time_offset=%d\n", local_time_offset);
		return 0;
	}
}

/*****************************************************************************/
/*  Public API implementation section.
-----------------------------------------------------------------------------*/

/*****************************************************************************/
/*
This function is used to open and initialize the FIOMAN.
*/
/*****************************************************************************/

/* fio_register() */
int
fioman_open
(
	struct inode		*inode,		/* INode of open */
	struct file		*filp		/* File structure for open */
)
{
	FIOMAN_PRIV_DATA	*p_priv;	/* Ptr to private App data */

#ifdef LAZY_CLOSE
        // Release the resources of all closed apps private data
	struct list_head	*p_app_elem;	/* Ptr to app element being examined */
	struct list_head	*p_app_next;	/* Temp Ptr to next for loop */
	FIOMAN_APP_FIOD		*p_app_fiod;	/* Ptr to app fiod structure */
        struct list_head        *p_priv_elem;
        struct list_head        *p_priv_next;
        FIOMAN_PRIV_DATA        *p_priv_old;
        
        p_priv_elem = NULL;
        list_for_each_safe( p_priv_elem, p_priv_next, &priv_list)
        {
		/* Get a ptr to this list entry */
		p_priv_old = list_entry( p_priv_elem, FIOMAN_PRIV_DATA, elem );

                if (timer_pending(&p_priv_old->hm_timer))
                        del_timer(&p_priv_old->hm_timer);
                        
                /* Deregister every FIOD still registered */
                list_for_each_safe( p_app_elem, p_app_next, &p_priv_old->fiod_list )
                {
                        /* Get a ptr to this list entry */
                        p_app_fiod = list_entry( p_app_elem, FIOMAN_APP_FIOD, elem );

                        /* Do the deregister for this FIOD */
                        fioman_do_dereg_fiod( p_app_fiod );
                }
		/* Delete this entry */
		list_del_init( p_priv_elem );

		/* Free the memory */
		kfree( p_priv_old );
        }
#endif
	/* Allocate our new App private data */
	if ( ! ( p_priv = (FIOMAN_PRIV_DATA *)kmalloc( sizeof( FIOMAN_PRIV_DATA ), GFP_KERNEL ) ) )
	{
		/* Failed to kmalloc private data */
		return ( -ENOMEM );
	}

	/* Initialize the fiod list */
	INIT_LIST_HEAD( &p_priv->fiod_list );
	
	/* Initialize hm timer for this app */
        init_timer(&p_priv->hm_timer);
	p_priv->hm_timer.function = hm_timer_alarm;
	p_priv->hm_timer.data = (unsigned long)p_priv;
        p_priv->hm_timeout = 0;
        p_priv->hm_fault = false;
        p_priv->transaction_in_progress = false;
        
        /* Initialize frame notification fifo */
        FIOMAN_FIFO_ALLOC(p_priv->frame_notification_fifo, sizeof(FIO_NOTIFY_INFO)*8, GFP_KERNEL);

	/* Save ptr so that fio_api calls have access */
	filp->private_data = p_priv;
		
	/* Show success */
	return (0);
}

/*****************************************************************************/

/*****************************************************************************/
/*
This function is used to close and cleanup FIOMAN
*/
/*****************************************************************************/

/* fio_deregister() */

int
fioman_release
(
	struct inode		*inode,		/* INode of open */
	struct file		*filp		/* File structure for open */
)
{
	FIOMAN_PRIV_DATA	*p_priv;		/* APP Private Data */
	struct list_head	*p_app_elem;	/* Ptr to app element being examined */
	struct list_head	*p_next;		/* Temp Ptr to next for loop */
	FIOMAN_APP_FIOD		*p_app_fiod;	/* Ptr to app fiod structure */

	/* Get the APPs private data */
	p_priv = (FIOMAN_PRIV_DATA *)filp->private_data;
#ifdef LAZY_CLOSE
        if (p_priv->hm_timeout != 0) {
                /* detach and save this priv data struct until hm timeout */
                list_add_tail(&p_priv->elem, &priv_list);
                
                return 0;
        } 
#endif
pr_debug("fioman_release: cancel hm timer for app %p\n", p_priv);
        if (timer_pending(&p_priv->hm_timer))
                del_timer(&p_priv->hm_timer);

	/* Deregister every FIOD still registered */
	list_for_each_safe( p_app_elem, p_next, &p_priv->fiod_list )
	{
		/* Get a ptr to this list entry */
		p_app_fiod = list_entry( p_app_elem, FIOMAN_APP_FIOD, elem );

		/* Do the deregister for this FIOD */
		fioman_do_dereg_fiod( p_app_fiod );
	}

        /* Free frame notification fifo */
	FIOMAN_FIFO_FREE(p_priv->frame_notification_fifo);
	
	/* free allocated memory */
	kfree( filp->private_data );

	/* Show success */
	return (0);
}

/*****************************************************************************/

/*****************************************************************************/
/*
This function is the ioctl interface for the FIOMAN psuedo driver.
*/
/*****************************************************************************/

long
fioman_ioctl
(
	struct file	*filp,	/* File Pointer */
	unsigned int	cmd,	/* Command associated with this call */
	unsigned long	arg	/* Argument for this command */
)
{
	int rt_code = 0;		/* IOCTL return code */

	/* Switch on this command */
	switch ( cmd )
	{
		case FIOMAN_IOC_REG_FIOD:
		{
			FIO_IOC_FIOD	*p_arg = (FIO_IOC_FIOD *)arg;
			FIOMSG_PORT	*p_port;

			/*pr_debug( "fioman_ioctl REG_FIOD\n" );*/
			/* Error checking */
			if ( ( (0 > p_arg->port) || (FIO_PORTS_MAX <= p_arg->port) )
				|| ( (0 > p_arg->fiod) || (FIOD_MAX <= p_arg->fiod) )
				/*|| ( !fio_port_table[p_arg->port].port_opened )*/ ) {
				/* Invalid argument */
				return ( -EINVAL );
			}

			/* Lock this port */
			p_port = &fio_port_table[p_arg->port];
			if ( down_interruptible( &p_port->sema ) ) {
				/* Interrupted */
				return ( -ERESTARTSYS );
			}

			rt_code = fioman_reg_fiod( filp, p_arg );

			/* Unlock this port */
			/* TEG - TODO */
			up( &p_port->sema );

			break;
		}

		case FIOMAN_IOC_DEREG_FIOD:
		{
			FIO_DEV_HANDLE	dev_handle = (FIO_DEV_HANDLE)arg;

			/*printk( KERN_ALERT "fioman_ioctl DEREG_FIOD\n" );*/

			rt_code = fioman_dereg_fiod( filp, dev_handle );

			break;
		}

		/* Enable Communications for a FIOD on a port */
		case FIOMAN_IOC_ENABLE_FIOD:
		{
			FIO_DEV_HANDLE	dev_handle = (FIO_DEV_HANDLE)arg;

			/*printk( KERN_ALERT "fioman_ioctl ENABLE_FIOD\n" );*/

			rt_code = fioman_enable_fiod( filp, dev_handle );

			break;
		}

		/* Disable Communications for a FIOD on a port */
		case FIOMAN_IOC_DISABLE_FIOD:
		{
			FIO_DEV_HANDLE	dev_handle = (FIO_DEV_HANDLE)arg;

			/*printk( KERN_ALERT "fioman_ioctl DISABLE_FIOD\n" );*/

			rt_code = fioman_disable_fiod( filp, dev_handle );

			break;
		}

		case FIOMAN_IOC_QUERY_FIOD:
		{
			FIO_IOC_FIOD	*p_arg = (FIO_IOC_FIOD *)arg;
			FIOMSG_PORT	*p_port;

			/*printk( KERN_ALERT "fioman_ioctl REG_FIOD\n" );*/
			/* Error checking */
			if ( ( (0 > p_arg->port) || (FIO_PORTS_MAX <= p_arg->port) )
				|| ( (0 > p_arg->fiod) || (FIOD_MAX <= p_arg->fiod) )
				/*|| ( !fio_port_table[p_arg->port].port_opened )*/ ) {
				/* Invalid argument */
				return ( -EINVAL );
			}

			/* Lock this port */
			p_port = &fio_port_table[p_arg->port];
			if ( down_interruptible( &p_port->sema ) ) {
				/* Interrupted */
				return ( -ERESTARTSYS );
			}

			rt_code = fioman_query_fiod( filp, p_arg );

			/* Unlock this port */
			/* TEG - TODO */
			up( &p_port->sema );

			break;
		}

		/* Set Output Point Reservations */
		case FIOMAN_IOC_RESERVE_SET:
		{
			FIO_IOC_RESERVE_SET	*p_arg = (FIO_IOC_RESERVE_SET *)arg;

			/*printk( KERN_ALERT "fioman_ioctl RESERVE_SET\n" );*/

			rt_code = fioman_reserve_set( filp, p_arg );

			break;
		}

		/* Get Output Point Reservations */
		case FIOMAN_IOC_RESERVE_GET:
		{
			FIO_IOC_RESERVE_GET	*p_arg = (FIO_IOC_RESERVE_GET *)arg;

			/*printk( KERN_ALERT "fioman_ioctl RESERVE_GET\n" );*/

			rt_code = fioman_reserve_get( filp, p_arg );

			break;
		}

		/* Set Output Point */
		case FIOMAN_IOC_OUTPUTS_SET:
		{
			FIO_IOC_OUTPUTS_SET	*p_arg = (FIO_IOC_OUTPUTS_SET *)arg;

			/*printk( KERN_ALERT "fioman_ioctl OUTPUTS_SET\n" );*/

			rt_code = fioman_outputs_set( filp, p_arg );

			break;
		}

		/* Get Output Points */
		case FIOMAN_IOC_OUTPUTS_GET:
		{
			FIO_IOC_OUTPUTS_GET	*p_arg = (FIO_IOC_OUTPUTS_GET *)arg;

			/*printk( KERN_ALERT "fioman_ioctl OUTPUTS_GET\n" );*/

			rt_code = fioman_outputs_get( filp, p_arg );

			break;
		}

		/* Set Channel Reservations */
		case FIOMAN_IOC_CHANNEL_RESERVE_SET:
		{
			FIO_IOC_CHANNEL_RESERVE_SET	*p_arg = (FIO_IOC_CHANNEL_RESERVE_SET *)arg;

			pr_debug( "fioman_ioctl CHANNEL_RESERVE_SET\n" );

			rt_code = fioman_channel_reserve_set( filp, p_arg );

			break;
		}

		/* Get Channel Reservations */
		case FIOMAN_IOC_CHANNEL_RESERVE_GET:
		{
			FIO_IOC_CHANNEL_RESERVE_GET	*p_arg = (FIO_IOC_CHANNEL_RESERVE_GET *)arg;

			pr_debug( "fioman_ioctl CHANNEL_RESERVE_GET\n" );

			rt_code = fioman_channel_reserve_get( filp, p_arg );

			break;
		}

		/* Set Channel Map */
		case FIOMAN_IOC_CHAN_MAP_SET:
		{
			FIO_IOC_CHAN_MAP_SET	*p_arg = (FIO_IOC_CHAN_MAP_SET *)arg;

			/*printk( KERN_ALERT "fioman_ioctl CHANNEL_MAP_SET\n" );*/

			rt_code = fioman_channel_map_set( filp, p_arg );

			break;
		}

		/* Get Channel Map */
		case FIOMAN_IOC_CHAN_MAP_GET:
		{
			FIO_IOC_CHAN_MAP_GET	*p_arg = (FIO_IOC_CHAN_MAP_GET *)arg;

			/*printk( KERN_ALERT "fioman_ioctl CHANNEL_MAP_GET\n" );*/

			rt_code = fioman_channel_map_get( filp, p_arg );

			break;
		}

		/* Get Channel Map Count */
		case FIOMAN_IOC_CHAN_MAP_COUNT:
		{
			FIO_IOC_CHAN_MAP_COUNT	*p_arg = (FIO_IOC_CHAN_MAP_COUNT *)arg;

			/*printk( KERN_ALERT "fioman_ioctl CHANNEL_MAP_COUNT\n" );*/

			rt_code = fioman_channel_map_count( filp, p_arg );

			break;
		}

		/* Set MMU flash bit */
		case FIOMAN_IOC_MMU_FLASH_BIT_SET:
		{
			FIO_IOC_MMU_FLASH_BIT_SET	*p_arg = (FIO_IOC_MMU_FLASH_BIT_SET *)arg;

			/*printk( KERN_ALERT "fioman_ioctl MMU_FLASH_BIT_SET\n" );*/

			rt_code = fioman_mmu_flash_bit_set( filp, p_arg );

			break;
		}

		/* Get MMU flash bit */
		case FIOMAN_IOC_MMU_FLASH_BIT_GET:
		{
			FIO_IOC_MMU_FLASH_BIT_GET *p_arg = (FIO_IOC_MMU_FLASH_BIT_GET *)arg;

			/*printk( KERN_ALERT "fioman_ioctl MMU_FLASH_BIT_GET\n" );*/

			rt_code = fioman_mmu_flash_bit_get( filp, p_arg );

			break;
		}

		/* Set Fault Monitor state */
		case FIOMAN_IOC_TS_FMS_SET:
		{
			FIO_IOC_TS_FMS_SET *p_arg = (FIO_IOC_TS_FMS_SET *)arg;

			/*printk( KERN_ALERT "fioman_ioctl TS_FAULT_MONITOR_SET\n" );*/

			rt_code = fioman_ts_fault_monitor_set( filp, p_arg );

			break;
		}

		/* Get Fault Monitor state */
		case FIOMAN_IOC_TS_FMS_GET:
		{
			FIO_IOC_TS_FMS_GET *p_arg = (FIO_IOC_TS_FMS_GET *)arg;

			/*printk( KERN_ALERT "fioman_ioctl TS_FAULT_MONITOR_GET\n" );*/

			rt_code = fioman_ts_fault_monitor_get( filp, p_arg );

			break;
		}

		/* Get CMU Configuration change count */
		case FIOMAN_IOC_CMU_CFG_CHANGE_COUNT:
		{
			FIO_DEV_HANDLE	p_arg = (FIO_DEV_HANDLE)arg;

			/*printk( KERN_ALERT "fioman_ioctl CMU_CFG_CHANGE_COUNT\n" );*/

			rt_code = fioman_cmu_config_change_count( filp, p_arg );

			break;
		}

		/* Set CMU Dark Channel Mask */
		case FIOMAN_IOC_CMU_DC_SET:
		{
			FIO_IOC_CMU_DC_SET	*p_arg = (FIO_IOC_CMU_DC_SET *)arg;

			/*printk( KERN_ALERT "fioman_ioctl CMU_DC_SET\n" );*/

			rt_code = fioman_cmu_dark_channel_set( filp, p_arg );

			break;
		}

		/* Get CMU Dark Channel Mask */
		case FIOMAN_IOC_CMU_DC_GET:
		{
			FIO_IOC_CMU_DC_GET	*p_arg = (FIO_IOC_CMU_DC_GET *)arg;

			/*printk( KERN_ALERT "fioman_ioctl CMU_DC_GET\n" );*/

			rt_code = fioman_cmu_dark_channel_get( filp, p_arg );

			break;
		}

		/* Set CMU Failed State (Fault) Action */
		case FIOMAN_IOC_CMU_FSA_SET:
		{
			FIO_IOC_CMU_FSA_SET	*p_arg = (FIO_IOC_CMU_FSA_SET *)arg;

			/*printk( KERN_ALERT "fioman_ioctl CMU_FSA_SET\n" );*/

			rt_code = fioman_cmu_fault_set( filp, p_arg );

			break;
		}

		/* Get CMU Failed State (Fault) Action */
		case FIOMAN_IOC_CMU_FSA_GET:
		{
			FIO_IOC_CMU_FSA_GET	*p_arg = (FIO_IOC_CMU_FSA_GET *)arg;

			/*printk( KERN_ALERT "fioman_ioctl CMU_FSA_GET\n" );*/

			rt_code = fioman_cmu_fault_get( filp, p_arg );

			break;
		}

		/* Get Input Points */
		case FIOMAN_IOC_INPUTS_GET:
		{
			FIO_IOC_INPUTS_GET	*p_arg = (FIO_IOC_INPUTS_GET *)arg;

			/*printk( KERN_ALERT "fioman_ioctl INPUTS_GET\n" );*/

			rt_code = fioman_inputs_get( filp, p_arg );

			break;
		}

		/* Get FIOD Status */
		case FIOMAN_IOC_FIOD_STATUS_GET:
		{
			FIO_IOC_FIOD_STATUS	*p_arg = (FIO_IOC_FIOD_STATUS *)arg;

			/*printk( KERN_ALERT "fioman_ioctl FIOD_STATUS_GET\n" );*/

			rt_code = fioman_fiod_status_get( filp, p_arg );

			break;
		}

		/* Reset FIOD Status */
		case FIOMAN_IOC_FIOD_STATUS_RESET:
		{
			FIO_DEV_HANDLE	dev_handle = (FIO_DEV_HANDLE)arg;

			/*printk( KERN_ALERT "fioman_ioctl FIOD_STATUS_RESET\n" );*/

			rt_code = fioman_fiod_status_reset( filp, dev_handle );

			break;
		}

		/* Read Frame Data */
		case FIOMAN_IOC_FIOD_FRAME_READ:
		{
			FIO_IOC_FIOD_FRAME_READ	*p_arg = (FIO_IOC_FIOD_FRAME_READ *)arg;

			/*printk( KERN_ALERT "fioman_ioctl FIOD_FRAME_READ\n" );*/

			rt_code = fioman_fiod_frame_read( filp, p_arg );

			break;
		}

		/* Write Frame Data */
		case FIOMAN_IOC_FIOD_FRAME_WRITE:
		{
			FIO_IOC_FIOD_FRAME_WRITE *p_arg = (FIO_IOC_FIOD_FRAME_WRITE *)arg;

			/*printk( KERN_ALERT "fioman_ioctl FIOD_FRAME_WRITE\n" );*/

			rt_code = fioman_fiod_frame_write( filp, p_arg );

			break;
		}

		/* Read Frame Size */
		case FIOMAN_IOC_FIOD_FRAME_SIZE:
		{
			FIO_IOC_FIOD_FRAME_SIZE	*p_arg = (FIO_IOC_FIOD_FRAME_SIZE *)arg;

			/*printk( KERN_ALERT "fioman_ioctl FIOD_FRAME_SIZE\n" );*/

			rt_code = fioman_fiod_frame_size( filp, p_arg );

			break;
		}

		/* Register Frame Notification */
		case FIOMAN_IOC_FIOD_FRAME_NOTIFY_REG:
		{
			FIO_IOC_FIOD_FRAME_NOTIFY_REG *p_arg = (FIO_IOC_FIOD_FRAME_NOTIFY_REG *)arg;

			/*printk( KERN_ALERT "fioman_ioctl FIOD_FRAME_NOTIFY_REG\n" );*/

			rt_code = fioman_fiod_frame_notify_register( filp, p_arg );

			break;
		}

		/* Deregister Frame Notification */
		case FIOMAN_IOC_FIOD_FRAME_NOTIFY_DEREG:
		{
			FIO_IOC_FIOD_FRAME_NOTIFY_DEREG *p_arg = (FIO_IOC_FIOD_FRAME_NOTIFY_DEREG *)arg;

			/*printk( KERN_ALERT "fioman_ioctl FIOD_FRAME_NOTIFY_DEREG\n" );*/

			rt_code = fioman_fiod_frame_notify_deregister( filp, p_arg );

			break;
		}

		/* Deregister Frame Notification */
		case FIOMAN_IOC_QUERY_NOTIFY_INFO:
		{
			FIO_IOC_QUERY_NOTIFY_INFO *p_arg = (FIO_IOC_QUERY_NOTIFY_INFO *)arg;

			/*printk( KERN_ALERT "fioman_ioctl FIOD_FRAME_NOTIFY_DEREG\n" );*/

			rt_code = fioman_query_frame_notify_status( filp, p_arg );

			break;
		}

		/* Set Local Time Offset */
		case FIOMAN_IOC_SET_LOCAL_TIME_OFFSET:
		{
			pr_debug( "fioman_ioctl SET_LOCAL_TIME_OFFSET\n" );
			rt_code = fioman_set_local_time_offset( filp, (int *)arg );

			break;
		}

		/* Set Volt Monitor state */
		case FIOMAN_IOC_TS1_VM_SET:
		{
			FIO_IOC_TS1_VM_SET *p_arg = (FIO_IOC_TS1_VM_SET *)arg;

			/*printk( KERN_ALERT "fioman_ioctl TS1_VOLT_MONITOR_SET\n" );*/

			rt_code = fioman_ts1_volt_monitor_set( filp, p_arg );

			break;
		}

		/* Get Volt Monitor state */
		case FIOMAN_IOC_TS1_VM_GET:
		{
			FIO_IOC_TS1_VM_GET *p_arg = (FIO_IOC_TS1_VM_GET *)arg;

			/*printk( KERN_ALERT "fioman_ioctl TS1_VOLT_MONITOR_GET\n" );*/

			rt_code = fioman_ts1_volt_monitor_get( filp, p_arg );

			break;
		}

#ifdef TS2_PORT1_STATE
		/* Request for TS2 port1 disable state */
		case FIOMAN_IOC_TS2_PORT1_STATE:
		{
			FIO_IOC_TS2_PORT1_STATE *p_arg = (FIO_IOC_TS2_PORT1_STATE *)arg;

			printk( KERN_ALERT "fioman_ioctl TS2_PORT1_STATE\n" );
			rt_code = fioman_ts2_port1_state( filp, p_arg );

			break;
		}
#endif
		/* Set Frame Schedule list */
		case FIOMAN_IOC_FIOD_FRAME_SCHD_SET:
		{
			FIO_IOC_FIOD_FRAME_SCHD_SET *p_arg = (FIO_IOC_FIOD_FRAME_SCHD_SET *)arg;

			/*printk( KERN_ALERT "fioman_ioctl FIOD_FRAME_SCHD_SET\n" );*/

			rt_code = fioman_frame_schedule_set( filp, p_arg );

			break;
		}

		/* Get Frame Schedule list */
		case FIOMAN_IOC_FIOD_FRAME_SCHD_GET:
		{
			FIO_IOC_FIOD_FRAME_SCHD_GET *p_arg = (FIO_IOC_FIOD_FRAME_SCHD_GET *)arg;

			/*printk( KERN_ALERT "fioman_ioctl FIOD_FRAME_SCHD_GET\n" );*/

			rt_code = fioman_frame_schedule_get( filp, p_arg );

			break;
		}

		/* Set Input Filter Values */
		case FIOMAN_IOC_FIOD_INPUTS_FILTER_SET:
		{
			FIO_IOC_FIOD_INPUTS_FILTER_SET *p_arg = (FIO_IOC_FIOD_INPUTS_FILTER_SET *)arg;

			/*printk( KERN_ALERT "fioman_ioctl FIOD_INPUTS_FILTER_SET\n" );*/

			rt_code = fioman_inputs_filter_set( filp, p_arg );

			break;
		}

		/* Get Input Filter Values */
		case FIOMAN_IOC_FIOD_INPUTS_FILTER_GET:
		{
			FIO_IOC_FIOD_INPUTS_FILTER_GET *p_arg = (FIO_IOC_FIOD_INPUTS_FILTER_GET *)arg;

			/*printk( KERN_ALERT "fioman_ioctl FIOD_INPUTS_FILTER_GET\n" );*/

			rt_code = fioman_inputs_filter_get( filp, p_arg );

			break;
		}
		
		/* Set Input Transitions to be monitored */
		case FIOMAN_IOC_INPUTS_TRANS_SET:
		{
			FIO_IOC_INPUTS_TRANS_SET *p_arg = (FIO_IOC_INPUTS_TRANS_SET *)arg;

			/*printk( KERN_ALERT "fioman_ioctl FIOD_INPUTS_TRANS_SET\n" );*/

			rt_code = fioman_inputs_trans_set( filp, p_arg );

			break;
		}

		/* Get Input Transitions being monitored */
		case FIOMAN_IOC_INPUTS_TRANS_GET:
		{
			FIO_IOC_INPUTS_TRANS_GET *p_arg = (FIO_IOC_INPUTS_TRANS_GET *)arg;

			/*printk( KERN_ALERT "fioman_ioctl FIOD_INPUTS_TRANS_GET\n" );*/

			rt_code = fioman_inputs_trans_get( filp, p_arg );

			break;
		}
		
		/* Get Input transition buffer entries */
		case FIOMAN_IOC_INPUTS_TRANS_READ:
		{
			FIO_IOC_INPUTS_TRANS_READ *p_arg = (FIO_IOC_INPUTS_TRANS_READ *)arg;

			/*printk( KERN_ALERT "fioman_ioctl FIOD_INPUTS_TRANS_READ\n" );*/

			rt_code = fioman_inputs_trans_read( filp, p_arg );

			break;
		}
		
		/* Register for watchdog service */
		case FIOMAN_IOC_WD_REG:
		{
			FIO_DEV_HANDLE	p_arg = (FIO_DEV_HANDLE)arg;

			/*printk( KERN_ALERT "fioman_ioctl CMU_WD_REG\n" );*/

			rt_code = fioman_wd_register( filp, p_arg );

			break;
		}

		/* Deregister from watchdog service */
		case FIOMAN_IOC_WD_DEREG:
		{
			FIO_DEV_HANDLE	p_arg = (FIO_DEV_HANDLE)arg;

			/*printk( KERN_ALERT "fioman_ioctl CMU_WD_DEREG\n" );*/

			rt_code = fioman_wd_deregister( filp, p_arg );

			break;
		}

		/* Reserve watchdog output pin */
		case FIOMAN_IOC_WD_RES_SET:
		{
			FIO_IOC_FIOD_WD_RES_SET	*p_arg = (FIO_IOC_FIOD_WD_RES_SET *)arg;

			/*printk( KERN_ALERT "fioman_ioctl CMU_WD_RES_SET\n" );*/

			rt_code = fioman_wd_reservation_set( filp, p_arg );

			break;
		}

		/* Return value of watchdog reserved output pin */
		case FIOMAN_IOC_WD_RES_GET:
		{
			FIO_IOC_FIOD_WD_RES_GET	*p_arg = (FIO_IOC_FIOD_WD_RES_GET *)arg;

			/*printk( KERN_ALERT "fioman_ioctl CMU_WD_RES_GET\n" );*/

			rt_code = fioman_wd_reservation_get( filp, p_arg );

			break;
		}

#ifdef NEW_WATCHDOG
		/* Set watchdog output toggle rate */
		case FIOMAN_IOC_WD_RATE_SET:
		{
			FIO_IOC_FIOD_WD_RATE_SET *p_arg = (FIO_IOC_FIOD_WD_RATE_SET *)arg;

			/*printk( KERN_ALERT "fioman_ioctl WD_RATE_SET\n" );*/

			rt_code = fioman_wd_rate_set( filp, p_arg );

			break;
		}
#endif
		/* Request toggle of watchdog output */
		case FIOMAN_IOC_WD_HB:
		{
			FIO_DEV_HANDLE	p_arg = (FIO_DEV_HANDLE)arg;
                        FIOMAN_PRIV_DATA *p_priv = filp->private_data;

			/*printk( KERN_ALERT "fioman_ioctl CMU_WD_HB\n" );*/

			rt_code = fioman_wd_heartbeat( p_priv, p_arg );

			break;
		}

		/* Get FIO driver API Version */
		case FIOMAN_IOC_VERSION_GET:
		{
			FIO_IOC_VERSION_GET *p_arg = (FIO_IOC_VERSION_GET *)arg;
			char ver[80] = "Intelight, 2.01, 2.17";
			
			if (copy_to_user(p_arg, ver, strlen(ver) )) {
				/* Could not copy for some reason */
				rt_code = -EFAULT;
			}
			break;
		}

		/* Register for health monitor service */
		case FIOMAN_IOC_HM_REG:
		{
			unsigned int	timeout = (unsigned int)arg;

			/*printk( KERN_ALERT "fioman_ioctl HM_REG\n" );*/

			rt_code = fioman_hm_register( filp, timeout );

			break;
		}

		/* Deregister from health monitor service */
		case FIOMAN_IOC_HM_DEREG:
		{
			/*printk( KERN_ALERT "fioman_ioctl HM_DEREG\n" );*/

			rt_code = fioman_hm_deregister( filp );

			break;
		}

		/* Heartbeat the health monitor */
		case FIOMAN_IOC_HM_HEARTBEAT:
		{
			/*printk( KERN_ALERT "fioman_ioctl HM_HEARTBEAT\n" );*/

			rt_code = fioman_hm_heartbeat( filp );

			break;
		}

		/* Reset the health monitor */
		case FIOMAN_IOC_HM_RESET:
		{
			/*printk( KERN_ALERT "fioman_ioctl HM_RESET\n" );*/

			rt_code = fioman_hm_fault_reset( filp );

			break;
		}

                case FIOMAN_IOC_TRANSACTION_BEGIN:
                        rt_code = fioman_transaction_begin( filp );
                        break;
                
                case FIOMAN_IOC_TRANSACTION_COMMIT:
                        rt_code = fioman_transaction_commit( filp );
                        break;
                        
		/* Undefined command */
		default:
		{
			/* Show invalid command passed */
			printk( KERN_ALERT "Invalid FIOMAN ioctl(%d)\n", cmd );
			rt_code = -EPERM;
			break;
		}
	}

	return ( rt_code );
}

/*****************************************************************************/

/*****************************************************************************/
/*
This function is used to initialize the FIOMAN.
*/
/*****************************************************************************/

void
fioman_init
(
	void
)
{
	/* FIOMSG INITIALIZATION */

	fiomsg_init();

	/*-------------------------------------------------------------------*/

	/* Ready list of FIOD / Port Combinations */
	INIT_LIST_HEAD( &fioman_fiod_list );

	/* Show first dev_handle to return */
	fioman_fiod_dev_next = 0;

        INIT_WORK(&hm_timeout_work, hm_timeout);
        
#ifdef LAZY_CLOSE
        INIT_LIST_HEAD( &priv_list );
#endif
}

/*****************************************************************************/

/*****************************************************************************/
/*
This function is used to clean up FIOMAN prior to unloading the driver.
*/
/*****************************************************************************/

void
fioman_exit
(
	void
)
{
	struct list_head	*p_sys_elem;	/* Element from FIOMAN FIOD list */
	struct list_head	*p_next;		/* Temp for loop */
	FIOMAN_SYS_FIOD		*p_sys_fiod;	/* Items to kill */

#ifdef LAZY_CLOSE
	struct list_head	*p_app_elem;	/* Ptr to app element being examined */
        struct list_head        *p_app_next;
	FIOMAN_APP_FIOD		*p_app_fiod;	/* Ptr to app fiod structure */
        struct list_head        *p_priv_elem;
        struct list_head        *p_priv_next;
        FIOMAN_PRIV_DATA        *p_priv_old;
        
        // Release the resources of priv_list
        list_for_each_safe( p_priv_elem, p_priv_next, &priv_list)
        {
		/* Get a ptr to this list entry */
		p_priv_old = list_entry( p_priv_elem, FIOMAN_PRIV_DATA, elem );

                if (timer_pending(&p_priv_old->hm_timer))
                        del_timer(&p_priv_old->hm_timer);
                        
                /* Deregister every FIOD still registered */
                list_for_each_safe( p_app_elem, p_app_next, &p_priv_old->fiod_list )
                {
                        /* Get a ptr to this list entry */
                        p_app_fiod = list_entry( p_app_elem, FIOMAN_APP_FIOD, elem );

                        /* Do the deregister for this FIOD */
                        fioman_do_dereg_fiod( p_app_fiod );
                }
		/* Delete this entry */
		list_del_init( p_priv_elem );

		/* Free the memory */
		kfree( p_priv_old );
        }
#endif
	/* Clean up the FIOMAN FIOD list -- should be empty (this is safety) */
	list_for_each_safe( p_sys_elem, p_next, &fioman_fiod_list )
	{
		/* Get a ptr to this list entry */
		p_sys_fiod = list_entry( p_sys_elem, FIOMAN_SYS_FIOD, elem );

		/* Delete this entry */
		list_del_init( p_sys_elem );

		/* Free the memory */
		kfree( p_sys_fiod );
	}

	/* Clean up FIOMSG */
	fiomsg_exit();
}

/*****************************************************************************/

/*****************************************************************************/
/*

REVISION HISTORY:
$Log$

*/
/*****************************************************************************/
