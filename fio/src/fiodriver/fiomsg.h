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

This include files contains all definitions and structures relative to the
FIO Message Scheduler (FIOMSG).

*/
/*****************************************************************************/


#ifndef _FIOMSG_H_
#define _FIOMSG_H_

/*  Include section.
-----------------------------------------------------------------------------*/
/* System includes. */
#include	<linux/list.h>		/* List definitions */
#include	<linux/ioctl.h>		/* IOCTL definitions */
#include	<linux/slab.h>		/* Memory Definitions */
#include        <linux/time.h>
#include        <linux/version.h>
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,26)
#include        <asm/semaphore.h>
#else
#include	<linux/semaphore.h>
#endif

/* Local includes. */
#include	"fiodriver.h"		/* FIO Driver Definitions */

/*  Definition section.
-----------------------------------------------------------------------------*/
/*  Module public structure/enum definition.*/

/* RX Buffer for port - One buffer per port.  Maximum size of a response */
#define	FIOMSG_RX_BUFFER		( 1024 )

/* Response frames are 128 greater than corresponding request frames */
#define	FIOMSG_RX_FRAME_NO(x)	( ( x ) + ( 128 ) )

/* Address associated with all stations */
#define	FIOMSG_ADDR_ALL			( 255 )
#define	FIOMSG_ADDR_TBD			( 1 )		/* TEG - TODO, real addr needed */

/* Timer definitions */
#if defined(CONFIG_HIGH_RES_TIMERS)
#include	<linux/hrtimer.h>
#if LINUX_VERSION_CODE < KERNEL_VERSION(3,11,0)
static inline ktime_t ktime_add_ms(const ktime_t kt, const u64 msec)
{
	return ktime_add_ns(kt, msec * NSEC_PER_MSEC);
}
static inline ktime_t ms_to_ktime(u64 ms)
{
	static const ktime_t ktime_zero = { .tv64 = 0 };
	
	return ktime_add_ms(ktime_zero, ms);
}
#endif
typedef ktime_t				FIOMSG_TIME;
typedef struct hrtimer			FIOMSG_TIMER;
#define FIOMSG_CLOCKS_PER_SEC		NSEC_PER_SEC
#define FIOMSG_CURRENT_TIME		ktime_get()
#define FIOMSG_TIME_TO_NSECS(a)         (long)ktime_to_ns(a)
#define	FIOMSG_TIME_AFTER_EQ(a,b)	((a).tv64 >= (b).tv64)
#define FIOMSG_TIME_AFTER(a,b)		((a).tv64 > (b).tv64)
#define FIOMSG_TIME_EQUAL(a,b)		ktime_equal(a,b)
#define FIOMSG_TIME_ADD_NSECS(a,b)	ktime_add_ns(a,b)
#define FIOMSG_TIME_ADD_USECS(a,b)	ktime_add_us(a,b)
#define FIOMSG_TIME_ADD(a,b)		ktime_add_ns(a,b)
#define FIOMSG_TIME_SUB(a,b)		ktime_sub(a,b)
#define FIOMSG_TIME_MOD(a,b)		((long)ktime_to_ns(a) % b)
#define FIOMSG_TIME_ALIGN(a,b)          ({ u32 _tmp = ktime_to_ms(a); \
                                                _tmp = _tmp - (_tmp % (MSEC_PER_SEC/b)); \
                                                a = ms_to_ktime(_tmp);})
#define FIOMSG_TIMER_INIT(a)		hrtimer_init(a,CLOCK_MONOTONIC,HRTIMER_MODE_ABS)
#define FIOMSG_TIMER_START(a,b)		hrtimer_start_range_ns(a,b,100000L,HRTIMER_MODE_ABS)
#define FIOMSG_TIMER_CANCEL(a)		hrtimer_cancel(a)
typedef enum hrtimer_restart		fiomsg_timer_callback_rtn;
typedef struct hrtimer*			fiomsg_timer_callback_arg;
#define FIOMSG_TIMER_CALLBACK_RTN	return(0)
#else
typedef	unsigned long			FIOMSG_TIME;
typedef struct timer_list		FIOMSG_TIMER;
#define FIOMSG_CLOCKS_PER_SEC		HZ
#define FIOMSG_CURRENT_TIME		(jiffies)
#define FIOMSG_TIME_TO_NSECS(a)         (jiffies_to_usecs(a) * NSEC_PER_USEC)
#define	FIOMSG_TIME_AFTER_EQ(a,b)	time_after_eq(a,b)
#define FIOMSG_TIME_AFTER(a,b)		time_after(a,b)
#define FIOMSG_TIME_EQUAL(a,b)		(a == b)
#define FIOMSG_TIME_ADD_NSECS(a,b)	(a + nsecs_to_jiffies(b))
#define FIOMSG_TIME_ADD_USECS(a,b)	(a + usecs_to_jiffies(b))
#define FIOMSG_TIME_ADD(a,b)		(a + b)
#define FIOMSG_TIME_SUB(a,b)		(a - b)
#define FIOMSG_TIME_MOD(a,b)		(a % b)
#define FIOMSG_TIME_ALIGN(a,b)          (a = a - (a % (HZ/b)))
#define FIOMSG_TIMER_INIT(a)		init_timer(a)
#define FIOMSG_TIMER_START(a,b)		({ (a)->expires = b; \
						(a)->data = (unsigned long)a; \
						add_timer(a);})
#define FIOMSG_TIMER_CANCEL(a)		del_timer_sync(a)
typedef void				fiomsg_timer_callback_rtn;
typedef unsigned long			fiomsg_timer_callback_arg;
#define FIOMSG_TIMER_CALLBACK_RTN	return
#endif

#define FIOMSG_TIME_BEFORE_EQ(a,b)	FIOMSG_TIME_AFTER_EQ(b,a)

/* The RX Pending structure is used in a toggling fashion.  The TX task */
/* will start working on the next TX before the previous RX is potentially */
/* processed.  Therefore. to avoid a potential race condition. there are 2 */
/* Pending RX timers used.  One is always available to the TX Task. */
#define	FIOMSG_RX_PEND_MAX		( 2 )

/* Structure utilized for Pending RX */
struct fiomsg_rx_pending
{
	u8			frame_no;		/* Expected response frame */
	int                     frame_len;              /* Length of frame received */
	FIO_IOC_FIOD		fiod;			/* FIOD that should respond */

	FIOMSG_TIMER		rx_timer;		/* RX Timer for pending resp */
};
typedef	struct fiomsg_rx_pending	FIOMSG_RX_PENDING;

/* Structure for managing port threads */
struct fiomsg_port
{
	struct semaphore	sema;				/* Semaphore for this port */
	bool			port_opened;			/* This port was opened */

	FIO_PORT		port;
	void			*context;			/* sdlc driver context */

	u16			comm_enabled;			/* Communications occurring */
	FIOMSG_TIME		start_time;			/* Timestamp of start of communications */

	FIOMSG_TIMER		tx_timer;			/* TX Timer for this port */
	struct list_head	tx_queue;			/* Queue of request frames */

	u16			tx_use_pend;			/* TX Use RX Pending, 0 - 1 */
	u16			rx_use_pend;			/* RX Use RX Pending, 0 - 1 */

								/* RX Pending Handlers */
	FIOMSG_RX_PENDING	rx_pend[FIOMSG_RX_PEND_MAX];
	struct list_head	rx_fiod_list[FIOD_MAX];		/* List of FIOD responses */
								/* RX Buffer for this port */
        wait_queue_head_t       read_wait;                      /* Wait queue for read frame calls */
        
	u8			rx_buffer[FIOMSG_RX_BUFFER];
};
typedef	struct fiomsg_port	FIOMSG_PORT;
#define	FIOMSG_P_PORT( port_no )	( &fio_port_table[ ( port_no ) ] )

/* Row entry for calculating dead times (wait times) */
/* All times listed as microseconds */
struct fiomsg_tx_dead_time_calc
{
	unsigned long		service_time;	/* FIOD service time for frane */
	unsigned long		response_time;	/* FIOD response time for frame */
	unsigned long		command_time;	/* CU command time for frame */
};
typedef	struct fiomsg_tx_dead_time_calc	FIOMSG_TX_DEAD_TIME_CALC;

/* Template for frame data */
struct fiomsg_frame
{
	u8			frame_addr;		/* Frame destination addr */
	u8			frame_ctrl;		/* Frame ctrl byte */
	u8			frame_no;		/* Number associated with this frame */
	u8			frame_info[1];		/* First byte of frame info */
};
typedef	struct fiomsg_frame	FIOMSG_FRAME;

/* Structure for Request Frames */
struct fiomsg_tx_frame
{
	struct list_head	elem;			/* Allow struct to be used in list */

	FIO_IOC_FIOD		fiod;			/* FIOD this frame is destined for */
	FIO_HZ			def_freq;		/* Default rate at which frame sent */
	FIO_HZ			cur_freq;		/* Current Rate frame is sent */
	FIOMSG_TIME		when;			/* When to send this frame */
							/* TX Frame update func to call */
	void			(*tx_func)( struct fiomsg_tx_frame * );
	void			*fioman_context;	/* Context for tx_func */
        struct fasync_struct    *notify_async_queue;
        
	bool			resp;			/* Is a response expected */
	u16			len;			/* Length of Request Frame */
	u8			frame[1];		/* Actual frame data */
};
typedef	struct fiomsg_tx_frame	FIOMSG_TX_FRAME;

/* Structure for Response Frames */
struct fiomsg_rx_frame
{
	struct list_head	elem;			/* Allow struct to be used in list */

	FIO_DEVICE_TYPE		fiod;			/* FIOD this frame was RX from */
	FIOMSG_TIME		when;			/* When frame was RX'ed */
	FIO_FRAME_INFO		info;
	struct fasync_struct	*notify_async_queue;
							/* RX Frame update func to call */
	void			(*rx_func)( struct fiomsg_rx_frame * );
	void			*fioman_context;	/* Context for rx_func */

	bool			resp;			/* Is a response stored here */
	u16			len;			/* Length of Response Frame */
	u8			frame[1];		/* Actual frame data */
};
typedef	struct fiomsg_rx_frame	FIOMSG_RX_FRAME;

/* Macro for payload access */
#define	FIOMSG_PAYLOAD( x )		((FIOMSG_FRAME *)( x->frame ))

/*  Global section.
-----------------------------------------------------------------------------*/


/*  Public Interface section.
-----------------------------------------------------------------------------*/

void fiomsg_init( void );
void fiomsg_exit( void );
void fiomsg_tx_remove_fiod( FIO_IOC_FIOD* );
void fiomsg_rx_remove_fiod( FIO_IOC_FIOD* );
void fiomsg_port_disable( FIO_IOC_FIOD* );
int fiomsg_port_comm_status( FIO_PORT );
int fiomsg_port_enable( FIOMSG_PORT* );
void fiomsg_tx_add_frame( FIOMSG_PORT*,	FIOMSG_TX_FRAME*);
void fiomsg_rx_add_frame( FIOMSG_PORT*, FIOMSG_RX_FRAME*);
int fiomsg_port_start( FIOMSG_PORT* );
void fiomsg_port_stop( FIOMSG_PORT* );
void fiomsg_port_close( FIOMSG_PORT* );

#endif /* #ifndef _FIOMSG_H_ */

/*****************************************************************************/
/*

REVISION HISTORY:
$Log$

*/
/*****************************************************************************/
