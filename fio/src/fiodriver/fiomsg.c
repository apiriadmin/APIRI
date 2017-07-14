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

This file contains all code pertinent to the FIO Message Scheduler (FIOMSG)
interface, as seen via the FIOMSG driver interface.

TEG - NO LOCKING IS IN PLACE.  THIS IS NOT AN ISSUE FOR INITIAL DEVELOPMENT
	  BUT MUST BE RESOLVE BEFORE RELEASE.

*/
/*****************************************************************************/


/*  Include section.
-----------------------------------------------------------------------------*/
/* System includes. */
#include	<linux/fs.h>		/* File System Definitions */
#include	<linux/poll.h>
#include	<linux/signal.h>
#include	<linux/sched.h>
#include	"atc_spxs.h"

/* Local includes. */
#include	"fiomsg.h"		/* FIOMSG Definitions					*/
#include        "fioman.h"

/*  Definition section.
-----------------------------------------------------------------------------*/
extern FIOMSG_PORT	fio_port_table[ FIO_PORTS_MAX ];

/* For logging purposes. */

/*  Global section.
-----------------------------------------------------------------------------*/

int fio_port[] =
{
	FIO_PORT_SP3,
	FIO_PORT_SP5,
	FIO_PORT_SP8
};

/*  Private API declaration (prototype) section.
-----------------------------------------------------------------------------*/

/* FIOMSG TX / RX tasks when kernel timers fires */
fiomsg_timer_callback_rtn fiomsg_tx_task( fiomsg_timer_callback_arg );
fiomsg_timer_callback_rtn fiomsg_rx_task( fiomsg_timer_callback_arg );

bool fiomsg_port_open( FIOMSG_PORT * );

/* ATC standard low-level driver functions */
extern void *sdlc_kernel_open( int channel );
extern int sdlc_kernel_close( void *context );
extern int sdlc_kernel_read( void *context, void *buf, size_t count);
extern size_t sdlc_kernel_write(void *context, const void *buf, size_t count);
extern int sdlc_kernel_ioctl(void *context, int command, unsigned long parameters);

/*  Public API interface section.
-----------------------------------------------------------------------------*/


/*  Private API implementation section.
-----------------------------------------------------------------------------*/
/*****************************************************************************/
/*
This function is used to insert a new request frame into the correct
sorted frame order.  The order is passed upon the "when" value for 
the frame to be inserted.  If a "when" value is equal to the "when"
value of an existing frame, the frame number is then used for the sort.
Thus, the sort order is "when" / frame number.
*/
/*****************************************************************************/

void
fiomsg_tx_add_frame
(
	FIOMSG_PORT		*p_port,	/* Port frame is to be sent on */
	FIOMSG_TX_FRAME	*p_tx_frame	/* Request Frame to queue */
)
{
	struct list_head	*p_elem;	/* Ptr to queue element being examined */
	FIOMSG_TX_FRAME		*p_tx_elem;	/* Ptr to tx frame being examined */

	/* For each element in the queue */
	list_for_each( p_elem, &p_port->tx_queue )
	{
		/* Get the request frame for this queue element */
		p_tx_elem = list_entry( p_elem, FIOMSG_TX_FRAME, elem );

		/* See if current element will expire after what is being added */
		if ( FIOMSG_TIME_AFTER_EQ(p_tx_elem->when, p_tx_frame->when) )
		{
			/* What is being added will expire before or equal to current */
			/* If current expires after new, OR the new frame is ordered
			   before the current frame -- insert the new frame here */
			if (   ( !FIOMSG_TIME_EQUAL(p_tx_elem->when, p_tx_frame->when) )
				|| ( FIOMSG_PAYLOAD( p_tx_elem )->frame_no >
					 FIOMSG_PAYLOAD( p_tx_frame )->frame_no ) )
			{
				/* Add the new frame at this point */
				/*if (FIOMSG_TIME_EQUAL(p_tx_elem->when, p_tx_frame->when)) {
					pr_debug(KERN_ALERT "tx_add_frame(#%d): when=%llu before #%d\n",
						FIOMSG_PAYLOAD(p_tx_frame)->frame_no,
						p_tx_frame->when.tv64, FIOMSG_PAYLOAD(p_tx_elem)->frame_no);
				}*/
				list_add_tail( &p_tx_frame->elem, p_elem );
				return;
			}
		}
	}
	/* New element belongs at end of queue */
	list_add_tail( &p_tx_frame->elem, &p_port->tx_queue );
}

/*****************************************************************************/

/*****************************************************************************/
/*
This function is used to set all output points for a FIOD to zero.
*/
/*****************************************************************************/

void
fiomsg_tx_set_outputs_0
(
	FIOMSG_PORT		*p_port,		/* Port of Request Queue */
	FIO_DEVICE_TYPE	fiod			/* FIOD to be removed */
)
{
/* TEG - TODO */
return;
}

/*****************************************************************************/

/*****************************************************************************/
/*
This function is used to remove all request frames destined for the indicated
FIOD from the Request Frame Queue.
*/
/*****************************************************************************/

void
fiomsg_tx_remove_fiod
(
	FIO_IOC_FIOD	*p_fiod			/* FIOD to be removed */
)
{
	struct list_head	*p_elem;	/* Ptr to queue element being examined */
	struct list_head	*p_next;	/* Temp Ptr to next for loop */
	FIOMSG_TX_FRAME		*p_tx_elem;	/* Ptr to tx frame being examined */
	FIOMSG_PORT			*p_port;	/* Port of Request Queue */

	/* Get port to work on */
	p_port = FIOMSG_P_PORT( p_fiod->port );

	/* For each element in the queue */
	list_for_each_safe( p_elem, p_next, &p_port->tx_queue )
	{
		/* Get the request frame for this queue element */
		p_tx_elem = list_entry( p_elem, FIOMSG_TX_FRAME, elem );

		/* See if current element is destined for FIOD interested in */
		if ( p_tx_elem->fiod.fiod == p_fiod->fiod )
		{
			/* YES, then remove this element */
			list_del_init( p_elem );

			/* Free memory with this TX buffer */
			kfree( p_tx_elem );
		}
	}
}

/*****************************************************************************/

/*****************************************************************************/
/*
This table is used to help calculate the dead time (wait time) to sending
of the next request frame.  All times are listed in microseconds and converted
to clock ticks by fiomsg_tx_next_when().
*/
/*****************************************************************************/

/* Values taken from NEMA-TS2 and ATC specifications where appropriate. */
/* Other values open for interpretation. */
/* Service/Response/Command Times */
static	FIOMSG_TX_DEAD_TIME_CALC	dead_time[] =
{
	{  2000/*was 500*/,  442, 1031 },	/*   0 - NEMA-TS2 */
	{  2000/*was 500*/, 1074,  360 },	/*   1 - NEMA-TS2 */
	{  500/*was 500*/, 1000,  500 },	/*   2 */
	{  2000/*was 500*/, 1705,  360 },	/*   3 - NEMA-TS2 */
	{  500/*was 500*/, 1000,  500 },	/*   4 */
	{  500/*was 500*/, 1000,  500 },	/*   5 */
	{  500/*was 500*/, 1000,  500 },	/*   6 */
	{  500/*was 500*/, 1000,  500 },	/*   7 */
	{  500/*was 500*/, 1000,  500 },	/*   8 */
	{   5000/*was 500*/,    0,  825 },	/*   9 - NEMA-TS2 */
	{  5000/*was 500*/,  758,  773 },	/*  10 - NEMA-TS2 */
	{  5000/*was 500*/,  758,  773 },	/*  11 - NEMA-TS2 */
	{  5000/*was 500*/,  758,  618 },	/*  12 - NEMA-TS2 */
	{  5000/*was 500*/,  758,  618 },	/*  13 - NEMA-TS2 */
	{  2000/*was 500*/, 1000,  500 },	/*  14 */
	{  2000/*was 500*/, 1000,  500 },	/*  15 */
	{  2000/*was 500*/, 1000,  500 },	/*  16 */
	{  500/*was 500*/, 1000,  500 },	/*  17 */
	{   2000/*was 500*/,    0,  360 },	/*  18 - NEMA-TS2 */
	{  500/*was 500*/, 1000,  500 },	/*  19 */
	{  2000/*was 500*/, 2715,  360 },	/*  20 - NEMA-TS2 */
	{  2000/*was 500*/, 2715,  360 },	/*  21 - NEMA-TS2 */
	{  2000/*was 500*/, 2715,  360 },	/*  22 - NEMA-TS2 */
	{  2000/*was 500*/, 2715,  360 },	/*  23 - NEMA-TS2 */
	{  500/*was 500*/, 1453,  412 },	/*  24 - NEMA-TS2 */
	{  500/*was 500*/, 1453,  412 },	/*  25 - NEMA-TS2 */
	{  500/*was 500*/, 1453,  412 },	/*  26 - NEMA-TS2 */
	{  500/*was 500*/, 1453,  412 },	/*  27 - NEMA-TS2 */
	{  500/*was 500*/, 1000,  500 },	/*  28 */
	{  500/*was 500*/, 1000,  500 },	/*  29 */
	{  500/*was 500*/, 1957,  360 },	/*  30 - NEMA-TS2 */
	{  500/*was 500*/, 1000,  500 },	/*  31 */
	{  500/*was 500*/, 1000,  500 },	/*  32 */
	{  500/*was 500*/, 1000,  500 },	/*  33 */
	{  500/*was 500*/, 1000,  500 },	/*  34 */
	{  500/*was 500*/, 1000,  500 },	/*  35 */
	{  500/*was 500*/, 1000,  500 },	/*  36 */
	{  500/*was 500*/, 1000,  500 },	/*  37 */
	{  500/*was 500*/, 1000,  500 },	/*  38 */
	{  500/*was 500*/, 1000,  500 },	/*  39 */
	{  500/*was 500*/, 2652,  360 },	/*  40 - NEMA-TS2 */
	{  500/*was 500*/, 1000,  500 },	/*  41 */
	{  500/*was 500*/,  442, 2166 },	/*  42 - NEMA-TS2 */
	{  500/*was 500*/,  442,  360 },	/*  43 - NEMA-TS2 */
	{  500/*was 500*/, 1000,  500 },	/*  44 */
	{  500/*was 500*/, 1000,  500 },	/*  45 */
	{  500/*was 500*/, 1000,  500 },	/*  46 */
	{  500/*was 500*/, 1000,  500 },	/*  47 */
	{  500/*was 500*/, 1000,  500 },	/*  48 */
	{  4000/*was 500*/, 1000,  275 },	/*  49 - ATC */
	{  4000/*was 500*/, 1000,  238 },	/*  50 - ATC */
	{  4000/*was 500*/, 1000,  6875 },	/*  51 - ATC */
	{  4000/*was 500*/, 1000,  320 },	/*  52 - ATC */
	{  4000/*was 500*/, 1000,  320 },	/*  53 - ATC */
	{  4000/*was 500*/, 1000,  10250 },	/*  54 - ATC */
	{  5000/*was 500*/, 1000,  410 },	/*  55 - ATC */
	{  4000/*was 500*/, 1000,  10250 },	/*  56 - ATC */
	{  4000/*was 500*/, 1000,  6875 },	/*  57 - ATC */
	{  4000/*was 500*/, 1000,  223 },	/*  58 - ATC */
	{  4000/*was 500*/, 1000,  223 },	/*  59 - ATC */
	{  4000/*was 500*/, 1000,  223 },	/*  60 - ATC */
	{  1000/*was 500*/, 1000,  500 },	/*  61 */
	{  1000/*was 500*/, 1000,  500 },	/*  62 */
	{  1000/*was 500*/, 1000,  320 },	/*  63 - ATC */
	{  1000/*was 500*/, 1000,  410 },	/*  64 - ATC */
	{  1000/*was 500*/, 1000,  500 },	/*  65 */
	{   500,    0,  825 },	/*  66 - ITS Cabinet Date / Time */
	{  1000/*was 500*/, 1000,  500 },	/*  67 */
	{  1000/*was 500*/, 1000,  500 },	/*  68 */
	{  1000/*was 500*/, 1000,  500 },	/*  69 */
	{  1000/*was 500*/, 1000,  500 },	/*  70 */
	{  1000/*was 500*/, 1000,  500 },	/*  71 */
	{  1000/*was 500*/, 1000,  500 },	/*  72 */
	{  1000/*was 500*/, 1000,  500 },	/*  73 */
	{  1000/*was 500*/, 1000,  500 },	/*  74 */
	{  1000/*was 500*/, 1000,  500 },	/*  75 */
	{  1000/*was 500*/, 1000,  500 },	/*  76 */
	{  1000/*was 500*/, 1000,  500 },	/*  77 */
	{  1000/*was 500*/, 1000,  500 },	/*  78 */
	{  1000/*was 500*/, 1000,  500 },	/*  79 */
	{  1000/*was 500*/, 1000,  500 },	/*  80 */
	{  1000/*was 500*/, 1000,  500 },	/*  81 */
	{  1000/*was 500*/, 1000,  500 },	/*  82 */
	{  1000/*was 500*/, 1000,  500 },	/*  83 */
	{  1000/*was 500*/, 1000,  500 },	/*  84 */
	{  1000/*was 500*/, 1000,  500 },	/*  85 */
	{  1000/*was 500*/, 1000,  500 },	/*  86 */
	{  1000/*was 500*/, 1000,  500 },	/*  87 */
	{  1000/*was 500*/, 1000,  500 },	/*  88 */
	{  1000/*was 500*/, 1000,  500 },	/*  89 */
	{  1000/*was 500*/, 1000,  500 },	/*  90 */
	{  1000/*was 500*/, 1000,  500 },	/*  91 */
	{  1000/*was 500*/, 1000,  500 },	/*  92 */
	{  1000/*was 500*/, 1000,  500 },	/*  93 */
	{  1000/*was 500*/, 1000,  500 },	/*  94 */
	{  1000/*was 500*/, 1000,  500 },	/*  95 */
	{  1000/*was 500*/, 1000,  500 },	/*  96 */
	{  1000/*was 500*/, 1000,  500 },	/*  97 */
	{  1000/*was 500*/, 1000,  500 },	/*  98 */
	{  1000/*was 500*/, 1000,  500 },	/*  99 */
	{  1000/*was 500*/, 1000,  500 },	/* 100 */
	{  1000/*was 500*/, 1000,  500 },	/* 101 */
	{  1000/*was 500*/, 1000,  500 },	/* 102 */
	{  1000/*was 500*/, 1000,  500 },	/* 103 */
	{  1000/*was 500*/, 1000,  500 },	/* 104 */
	{  1000/*was 500*/, 1000,  500 },	/* 105 */
	{  1000/*was 500*/, 1000,  500 },	/* 106 */
	{  1000/*was 500*/, 1000,  500 },	/* 107 */
	{  1000/*was 500*/, 1000,  500 },	/* 108 */
	{  1000/*was 500*/, 1000,  500 },	/* 109 */
	{  1000/*was 500*/, 1000,  500 },	/* 110 */
	{  1000/*was 500*/, 1000,  500 },	/* 111 */
	{  1000/*was 500*/, 1000,  500 },	/* 112 */
	{  1000/*was 500*/, 1000,  500 },	/* 113 */
	{  1000/*was 500*/, 1000,  500 },	/* 114 */
	{  1000/*was 500*/, 1000,  500 },	/* 115 */
	{  1000/*was 500*/, 1000,  500 },	/* 116 */
	{  1000/*was 500*/, 1000,  500 },	/* 117 */
	{  1000/*was 500*/, 1000,  500 },	/* 118 */
	{  1000/*was 500*/, 1000,  500 },	/* 119 */
	{  1000/*was 500*/, 1000,  500 },	/* 120 */
	{  1000/*was 500*/, 1000,  500 },	/* 121 */
	{  1000/*was 500*/, 1000,  500 },	/* 122 */
	{  1000/*was 500*/, 1000,  500 },	/* 123 */
	{  1000/*was 500*/, 1000,  500 },	/* 124 */
	{  1000/*was 500*/, 1000,  500 },	/* 125 */
	{  1000/*was 500*/, 1000,  500 },	/* 126 */
	{  1000/*was 500*/, 1000,  500 }	/* 127 */
};

/*****************************************************************************/
/*
This function is used to resolve when the next TX timer software interrupt
should occur.  This is based upon the formula, from the NEMA TS-2 Spec:

D[n] = S[n] + R[n] - C[n+1]

Where:
	D[n]   - Is the Dead Time (wait time) to sending the next request frame
	S[n]   - Is the Service Time of the FIOD for processing the request
	R[n]   - Is the Response Time of the transmission of the response
	C[n+1] - Is the Command Time of the next frame

To apply to this situation however, the following formula is also used:

TX = J + C[n] + D[n]

Where:
	TX   - TX is the new (absolute) TX Timer software interrupt time
	J    - Is the current Jiffies time (absolute)
	C[n] - Is the time to process the command currently being sent
	D[n] - Is the Dead Time (wait time) calculated above

This additional calculation above takes into account that the current command
has not been processed or sent yet.  Therefore, this time must be factored in.
TX represents the absolute time (in jiffies) before we can start sending the
next command frame.  This time takes into consideration that the next command
frame must not be fully sent until the previous command frames response has
been received -- 2 FIODs must not be processing commands and therefore
possibily sending responses at the same time.

The maximum of the calculated TX OR the next scheduled frame's "when" time
is returned.  This makes sure that the next frame is not sent before it is
supposed to be.

A value of true is returned if there is a new TX time, otherwise false.
*/
/*****************************************************************************/

bool
fiomsg_tx_next_when
(
	FIOMSG_PORT			*p_port,		/* Pointer to port being worked on */
	FIOMSG_TX_FRAME		*p_tx_current,	/* Frame being sent now */
	struct list_head	*next_elem,		/* Pointer to next TX queue element */
	FIOMSG_TIME			*next_when		/* Pointer to returned when */
)
{
	/* See if there is more work to be performed */
	if (   ( FIO_HZ_ONCE != p_tx_current->cur_freq )
		|| ( next_elem != &p_port->tx_queue ) )
	{
		/* There is more work to be performed */

		FIOMSG_TX_FRAME				*p_tx_next;	/* Ptr to next tx frame */
		FIOMSG_TX_DEAD_TIME_CALC	*p_c_dt;	/* Ptr to current dt row */
		FIOMSG_TX_DEAD_TIME_CALC	*p_n_dt;	/* Ptr to next dt row */
		long						us_dt;		/* Calc'ed dead_time in microsecs */
		FIOMSG_TIME					tmp_when;	/* For comparison */

		/* See if there is a "next" frame */
		if ( next_elem != &p_port->tx_queue )
		{
			/* There is at least one more frame */
			p_tx_next = list_entry( next_elem, FIOMSG_TX_FRAME, elem );
		}
		else
		{
			/* There is no next frame */
			/* This frame is currently the only frame and will be requeued */
			p_tx_next = p_tx_current;
		}

		/* start by setting next interval to the next frames "when" time */
		*next_when = p_tx_next->when;

		/* Now calculate interval */
		p_c_dt = &dead_time[ FIOMSG_PAYLOAD( p_tx_current )->frame_no ];
		p_n_dt = &dead_time[ FIOMSG_PAYLOAD( p_tx_next )->frame_no ];

		/* us_dt is the dead_time in microseconds */
		us_dt =	p_c_dt->service_time +		/* Current service time */
			    p_c_dt->response_time -	/* Current response time */
			    p_n_dt->command_time;	/* Next frame command time */

		us_dt = p_c_dt->command_time +	/* Current frame command time */
				max( 500L, us_dt);		/* TS2 minimum dead-time 0.5ms */

		/* This needs to be converted to jiffies now */
		/* Any fraction is considered to be another tick */
		/*tmp_when = ( ( us_dt + ( ( 1000000 / HZ ) - 1 ) ) / ( 1000000 / HZ ) );*/

		/* Now add in jiffies for an absolute time */
		/*tmp_when += FIOMSG_CURRENT_TIME;*/

		/* Calculate an absolute time */
		tmp_when = FIOMSG_TIME_ADD_USECS(FIOMSG_CURRENT_TIME, us_dt);

		/* If the calculated time with dead_time is after the next frames */
		/* scheduled transmission time; set the new TX timer to fire at */
		/* the calculated time. */
		if ( FIOMSG_TIME_AFTER( tmp_when, *next_when ) )
		{
			*next_when = tmp_when;
		}

		return ( true );
	}

	/* If no, then this frame is only frame in queue */
	/* And will be removed */
	/* Return 0 as next time */
	return ( false );
}

/*****************************************************************************/

/*****************************************************************************/
/*
This function is used to set up the TX Timer.
*/
/*****************************************************************************/

void
fiomsg_tx_set_timer
(
	FIOMSG_PORT		*p_port,	/* Port for which timer is to be set */
	FIOMSG_TIME		when		/* When to set timer to */
)
{
	FIOMSG_TIMER	*p_timer = &p_port->tx_timer;

	p_timer->function = fiomsg_tx_task;
	FIOMSG_TIMER_START(p_timer, when);
}

/*****************************************************************************/

/*****************************************************************************/
/*
This function is used to set the port TX timer for the indicated port.
*/
/*****************************************************************************/

void
fiomsg_tx_set_port_timer
(
	FIOMSG_PORT		*p_port,	/* Port for which timer is to be set */
	FIOMSG_TX_FRAME	*p_tx_frame	/* Current frame being sent */
)
{
	FIOMSG_TIME	next_when;	/* next time to fire */

	/* Figure out when the next TX timer should fire */
	/* If timer should be set for next time */
	if ( fiomsg_tx_next_when( p_port, p_tx_frame, p_tx_frame->elem.next, &next_when ) )
	{
		/* Set up time for next time */
		fiomsg_tx_set_timer( p_port, next_when );
	}
}

/*****************************************************************************/

/*****************************************************************************/
/*
This function is used to resolve when the next RX timer software interrupt
should occur.  This is based upon the formula:

RX = J + C[n] + S[n] + R[n]

Where:
	RX   - RX is the new (absolute) RX Timer software interrupt time
	J    - Is the current Jiffies time (absolute)
	C[n] - Is the time to process the command currently being sent
	S[n] - Is the Service Time of the FIOD for processing the request
	R[n] - Is the Response Time of the transmission of the response

This calculation takes into account that the current command
has not been processed or sent yet.  Therefore, this time must be factored in.
RX represents the absolute time (in jiffies) before we can poll for the
response frame.  This time takes into consideration that the next command
frame must not be fully sent until the this command frames response has
been received -- 2 FIODs must not be processing commands and therefore
possibily sending responses at the same time.

A value of true is returned if there is a new RX time, otherwise false.
*/
/*****************************************************************************/

bool
fiomsg_rx_next_when
(
	FIOMSG_PORT			*p_port,		/* Pointer to port being worked on */
	FIOMSG_TX_FRAME		*p_tx_current,	/* Frame being sent now */
	FIOMSG_TIME		*next_when		/* Pointer to returned value */
)
{
	/* See if a response is expected */
	if ( p_tx_current->resp )
	{
		/* There is a responce expected */

		FIOMSG_TX_DEAD_TIME_CALC	*p_c_dt;	/* Ptr to current dt row */
		unsigned long				us_dt;		/* Calc'ed dead_time in microsecs */

		/* Now calculate interval */
		p_c_dt = &dead_time[ FIOMSG_PAYLOAD( p_tx_current )->frame_no ];

		us_dt = p_c_dt->command_time +		/* Current frame command time */
				p_c_dt->service_time +	/* Current service time */
			    p_c_dt->response_time;	/* Current response time */

		/* us_dt is a the dead_time in microseconds */
		/* This needs to be converted to jiffies now */
		/* Any fraction is considered to be another tick */
		/**next_when = (( us_dt + ( ( 1000000 / HZ ) - 1 ) ) / ( 1000000 / HZ ) );*/

		/* Now add in jiffies for an absolute time */
		/**next_when += FIOMSG_CURRENT_TIME;*/
		*next_when = FIOMSG_TIME_ADD_USECS(FIOMSG_CURRENT_TIME, us_dt);

		/* Show RX time */
		return ( true );
	}

	/* No response is expected */
	/* Return 0 as next time */
	/* Return no RX timer */
	return ( false );
}

/*****************************************************************************/

/*****************************************************************************/
/*
This function is used to set the port RX timer for the indicated port.
*/
/*****************************************************************************/

void
fiomsg_rx_set_port_timer
(
	FIOMSG_PORT		*p_port,		/* Port for which timer is to be set */
	FIOMSG_TX_FRAME *p_tx_frame		/* TX frame to set response timer for */
)
{
	/* If a response is not expected, we will not utilize an RX Pend Buffer */
	/* Therefore, the current RX Pend buffer will be utilized for the */
	/* Next TX Frame. */
	/* As fiomsg_rx_task() will NOT be called in this case, the rx_use_pend */
	/* variable will not be modified either, so everything stays in sync. */

	FIOMSG_TIME		next_when;	/* next time to fire */

	/* Figure out when the next RX timer should fire */
	/* See if a response is expected */
	if ( fiomsg_rx_next_when( p_port, p_tx_frame, &next_when ) )
	{
		/* Show which RX Pend to use */
		FIOMSG_RX_PENDING	*p_rx_pend = &p_port->rx_pend[p_port->tx_use_pend];

		/* A response is expected, so set the response timer to poll */
		FIOMSG_TIMER	*p_timer = &p_rx_pend->rx_timer;

		/* Update RX Pend for next time */
		p_port->tx_use_pend = 1 - p_port->tx_use_pend;

		/* Show which response frame we are expecting */
		p_rx_pend->frame_no = FIOMSG_RX_FRAME_NO(
									FIOMSG_PAYLOAD( p_tx_frame )->frame_no );
		p_rx_pend->fiod = p_tx_frame->fiod;

		p_timer->function = fiomsg_rx_task;

		/* Set the timer to go off when the response should be totally RXed */
		FIOMSG_TIMER_START(p_timer,next_when);

		/* TEG DEL */
		/*pr_debug( KERN_ALERT "Setting RX timer, expiry %lu\n", p_timer->expires );*/
		/* TEG DEL */
	}

	return;
}

/*****************************************************************************/

/*****************************************************************************/
/*
This function is used to insert a new response frame into the correct
sorted frame order.  There is a seperate list for each FIOD for each port.
Inside these lists, the sort order is based upon the frame #.
This list is pre-built so that when the actual RX occurs, we can just drop
the response into place.
*/
/*****************************************************************************/

void
fiomsg_rx_add_frame
(
	FIOMSG_PORT		*p_port,		/* Port frame is to be sent on */
	FIOMSG_RX_FRAME	*p_frame		/* Response Frame to add */
)
{
	struct list_head	*p_elem;		/* Ptr to list element being examined */
	FIOMSG_RX_FRAME		*p_rx_elem;		/* Ptr to rx frame being examined */

	/* For each element in the list */
	list_for_each( p_elem, &p_port->rx_fiod_list[ p_frame->fiod ] )
	{
		/* Get the response frame for this queue element */
		p_rx_elem = list_entry( p_elem, FIOMSG_RX_FRAME, elem );

		/* See if current element frame no is greater than
		   RX frame being inserted */
		if ( FIOMSG_PAYLOAD( p_rx_elem )->frame_no >=
			 FIOMSG_PAYLOAD( p_frame )->frame_no )
		{
			/* If this frame already exists (which should never happen)
			   Kill this frame and don't add it */
			if ( FIOMSG_PAYLOAD( p_rx_elem )->frame_no ==
				 FIOMSG_PAYLOAD( p_frame )->frame_no )
			{
				pr_debug( KERN_ALERT
						"Trying to add existing RX frame(%d), killing!\n",
						FIOMSG_PAYLOAD( p_frame )->frame_no );
				kfree( p_frame );
				return;
			}
			else
			{
				/* Add the new frame at this point */
				list_add_tail( &p_frame->elem, p_elem );
				return;
			}
		}
	}
	/* New element belongs at end of list */
	list_add_tail( &p_frame->elem, &p_port->rx_fiod_list[ p_frame->fiod ] );
}

/*****************************************************************************/

void fiomsg_rx_notify( FIOMSG_RX_FRAME *frame, FIO_NOTIFY_INFO *notify_info )
{
        struct fasync_struct *fa = frame->notify_async_queue;
        FIOMAN_PRIV_DATA *priv;
	struct list_head	*p_app_elem;	/* Ptr to app element being examined */
	FIOMAN_APP_FIOD		*p_app_fiod;	/* Ptr to app fiod structure */

        /* Signal the queue */
        kill_fasync(&frame->notify_async_queue, SIGIO, POLL_IN);

        /* Add notify info to fifo for each member of queue */
        while (fa) {
		priv = (FIOMAN_PRIV_DATA *)fa->fa_file->private_data;
		/* Search list of app_fiods for this user to find FIO_DEV_HANDLE */
		list_for_each( p_app_elem, &priv->fiod_list ) {
			/* Get a ptr to this list entry */
			p_app_fiod = list_entry( p_app_elem, FIOMAN_APP_FIOD, elem );
			if (p_app_fiod->fiod.fiod == frame->fiod) {
				notify_info->fiod = p_app_fiod->dev_handle;
				break;
			}
		}
                FIOMAN_FIFO_PUT(priv->frame_notification_fifo, notify_info, sizeof(FIO_NOTIFY_INFO));
                /* remove from fasync queue if one-shot notify type */
                if (!FIO_BIT_TEST(p_app_fiod->frame_notify_type, notify_info->rx_frame))
			fasync_helper(0, fa->fa_file, 0, &frame->notify_async_queue);
		if (fa)
			fa = fa->fa_next;
        }

}
/*****************************************************************************/
/*
This function is used to update a RX frame that was just received.
*/
/*****************************************************************************/
void
fiomsg_rx_update_frame
(
	FIOMSG_PORT             *p_port,        /* Port RX frame was recieved on */
	FIOMSG_RX_PENDING       *p_rx_pend,     /* Pending structure utitlized */
	bool                    success         /* Indicates rx success or error */
)
{
	struct list_head	*p_elem;		/* Ptr to list element being examined */
	FIOMSG_RX_FRAME		*p_rx_elem;		/* Ptr to rx frame being examined */
        FIO_NOTIFY_INFO         notify_info;

	/* For each element in the list */
	list_for_each( p_elem, &p_port->rx_fiod_list[ p_rx_pend->fiod.fiod ] )
	{
		/* Get the response frame for this queue element */
		p_rx_elem = list_entry( p_elem, FIOMSG_RX_FRAME, elem );

		/* See if current element frame no is what we are looking for */
		if ( FIOMSG_PAYLOAD( p_rx_elem )->frame_no == p_rx_pend->frame_no )
		{
                        notify_info.rx_frame = p_rx_pend->frame_no;
			if (!success) {
				/* Just update error counts */
				p_rx_elem->resp = false;
				if (p_rx_elem->info.error_rx < 4294967295L)
					p_rx_elem->info.error_rx++;
				if (p_rx_elem->info.error_last_10 < 10)
					p_rx_elem->info.error_last_10++;
                                notify_info.status = FIO_FRAME_ERROR;
			} else {
                                /* Copy data into frame list */
                                memcpy( FIOMSG_PAYLOAD( p_rx_elem ),
					p_port->rx_buffer,
					p_rx_pend->frame_len );
				p_rx_elem->len = p_rx_pend->frame_len;
			
                                /* Update other information */
                                p_rx_elem->when = FIOMSG_CURRENT_TIME;
                                p_rx_elem->resp = true;
                                /* Increment frame sequence number */
                                p_rx_elem->info.last_seq++;
                                if (p_rx_elem->info.success_rx < 4294967295L)
					p_rx_elem->info.success_rx++;
                                if (p_rx_elem->info.error_last_10)
                                        p_rx_elem->info.error_last_10--;
                                notify_info.status = FIO_FRAME_RECEIVED;
                                notify_info.seq_number = p_rx_elem->info.last_seq;
                                notify_info.count = (p_rx_elem->len - 2);
                                /* Let FIOMAN do its work, if needed */
                                if ( NULL != p_rx_elem->rx_func ) {
                                        /* FIOMAN has work to do */
                                        (*p_rx_elem->rx_func)( p_rx_elem );
                                }
                        }
                        /* Update notification info for this frame/fiod */			
			/* and signal any users in notification queue */
			if (p_rx_elem->notify_async_queue != NULL)
                                fiomsg_rx_notify(p_rx_elem, &notify_info);
			
                        /* Wake up any waiting reads */
                        wake_up_interruptible(&p_port->read_wait);
                        
			return;
		}
	}

	printk( KERN_ALERT
			"RX Buffer dropped, fiod(%d), frame(%d) - no place to put\n",
			p_rx_pend->fiod.fiod, p_rx_pend->frame_no );
}

/*****************************************************************************/

/*****************************************************************************/
/*
This function is used to remove all response frames destined for the indicated
FIOD from the Response Frame List.
*/
/*****************************************************************************/

void
fiomsg_rx_remove_fiod
(
	FIO_IOC_FIOD	*p_fiod			/* FIOD to be removed */
)
{
	struct list_head	*p_elem;	/* Ptr to queue element being examined */
	struct list_head	*p_next;	/* Temp Ptr to next for loop */
	FIOMSG_RX_FRAME		*p_rx_elem;	/* Ptr to tx frame being examined */
	FIOMSG_PORT			*p_port;	/* Port of Request Queue */

	/* Get port to work on */
	p_port = FIOMSG_P_PORT( p_fiod->port );

	/* For each element in the queue */
	list_for_each_safe( p_elem, p_next, &p_port->rx_fiod_list[ p_fiod->fiod ] )
	{
		/* Get the request frame for this queue element */
		p_rx_elem = list_entry( p_elem, FIOMSG_RX_FRAME, elem );

		/* Remove all entires from this list */
		list_del_init( p_elem );

		/* Free memory with this RX buffer */
		kfree( p_rx_elem );
	}

	/* Re-init in case FIOD opened back up */
	INIT_LIST_HEAD( &p_port->rx_fiod_list[ p_fiod->fiod ] );
}

/*****************************************************************************/

/*****************************************************************************/
/*
This function is used to build an SDLC Request frame.
*/
/*****************************************************************************/

/* TEG - TODO */
int						/* Return buffer size								*/
build_tx_sdlc_frame
(
	u_char	address,	/* Destination address								*/
	u_char	type,		/* Type of request frame							*/
	u_char	*payload,	/* Data to send										*/
	u_char	*frame_buf	/* Buffer to generate frame into					*/
)
{
	/* Set return count size to 0 */

	/* Place address into frame */

	/* Place type into frame */

	/* Place CTRL info into frame */

	/* Copy payload into frame */

	/* return buffer size */
return (0);
}

/*****************************************************************************/

/*  Public API implementation section.
-----------------------------------------------------------------------------*/

/*****************************************************************************/
/*
This function tells whether or not communications are occurring to the passed
port.
*/
/*****************************************************************************/

int
fiomsg_port_comm_status
(
	FIO_IOC_FIOD	*p_fiod		/* FIOD being looked at */
)
{
	FIOMSG_PORT		*p_port;	/* Port on which to enable FIOD */

	/* Get the port */
	p_port = FIOMSG_P_PORT( p_fiod->port );

	/* Return number of APPS that have enabled comm */
	return ( p_port->comm_enabled );
}

/*****************************************************************************/

/*****************************************************************************/
/*
This function opens BSP serial port if required.
*/
/*****************************************************************************/

int
fiomsg_port_enable
(
	FIOMSG_PORT		*p_port	/* Port on which to enable FIOD */
)
{
pr_debug("fiomsg_port_enable: opening port %d\n", p_port->port);
	/* See if port has been opened */
	if ( !p_port->port_opened ) {
		/* Try to open the port if not open */
		if ( !(p_port->port_opened = fiomsg_port_open( p_port )) ) {
			printk( KERN_ALERT "FIOMSG failed to open port %d\n", p_port->port );
			/* Port was not opened */
			return ( -ENODEV );
		}
                p_port->tx_use_pend = p_port->rx_use_pend = 0;
		/* Set the start timestamp for this port */
		p_port->start_time = FIOMSG_CURRENT_TIME;
		printk( KERN_ALERT "FIOMSG Task port(%d) -> opened (%lu)\n", p_port->port, 
                        FIOMSG_TIME_TO_NSECS(p_port->start_time) );
	}
	return 0;
}

/*****************************************************************************/

/*****************************************************************************/
/*
This function handles starting comm on a port.
*/
/*****************************************************************************/

int
fiomsg_port_start
(
	FIOMSG_PORT		*p_port	/* Port on which to enable FIOD */
)
{

	/* Enable comm to port if first time */
	if ( ! ( p_port->comm_enabled++ ) )
	{
		printk( KERN_ALERT "TX timer being set up, to start things\n" );

		/* Set the TX timer to get things going */
		fiomsg_tx_set_timer( p_port, FIOMSG_CURRENT_TIME );
	}
	return 0;
}

/*****************************************************************************/

/*****************************************************************************/
/*
Stop comm to a port
*/
/*****************************************************************************/

void
fiomsg_port_stop
(
	FIOMSG_PORT	*p_port		/* Port to stop comm on */
)
{
	int		ii;		/* Loop variable */
pr_debug("fiomsg_port_stop: stopping port %d\n", p_port->port);

	/* Disable timers */
	FIOMSG_TIMER_CANCEL( &p_port->tx_timer );
	for ( ii = 0; ii < FIOMSG_RX_PEND_MAX; ii++ )
	{
		FIOMSG_TIMER_CANCEL( &p_port->rx_pend[ii].rx_timer );
	}
}

/*****************************************************************************/

/*****************************************************************************/
/*
This function handles diabling an FIOD on a port.
*/
/*****************************************************************************/

void
fiomsg_port_disable
(
	FIO_IOC_FIOD	*p_fiod		/* Disable FIOD Structrue */
)
{
	FIOMSG_PORT		*p_port;	/* Port on which to disable FIOD */

	/* get port table address */
	p_port = FIOMSG_P_PORT( p_fiod->port );
pr_debug("fiomsg_port_disable: disabling port %d, comm_enabled=%d\n", p_port->port, p_port->comm_enabled);

	/* Show comm is disabled on port, and disable port messages
	   if port disable is last time */
	if ( ! ( --p_port->comm_enabled ) )
	{
		/* Stop comm to this port */
		fiomsg_port_stop( p_port );
		fiomsg_port_close( p_port );
		p_port->port_opened = false;
		printk( KERN_ALERT "FIOMSG Task port(%d) -> closed\n",
			p_port->port );
	}
}

/*****************************************************************************/

/*****************************************************************************/
/*
This function is used to open and comfigure an ATC port for SDLC
communications to a FIOD.
*/
/*****************************************************************************/

bool						/* true = port opened, false = port not opened */
fiomsg_port_open
(
	FIOMSG_PORT		*p_port	/* Pointer to port info to open */
)
{
	int channel;
	void *context;
	atc_spxs_config_t config = {ATC_SDLC, ATC_B614400, ATC_CLK_INTERNAL, ATC_GATED};
/* TEG */
	/* Initialize */
	/* Open SDLC driver for indicated port */
	if(p_port->port == FIO_PORT_SP5)
		channel = ATC_LKM_SP5S;
        else if (p_port->port == FIO_PORT_SP8)
                channel = ATC_LKM_SP8S;
	else if (p_port->port == FIO_PORT_SP3) {
		channel = ATC_LKM_SP3S;
		config.baud = ATC_B153600;
	} else {
		printk("fio_port_open: unknown port %d {%d,%d}", p_port->port, FIO_PORT_SP3, FIO_PORT_SP5);
		return false;
	}
	if( IS_ERR( context = sdlc_kernel_open(channel) ) ) {
    	printk( "fio_port_open: open port %d: error %d", channel, (unsigned int)context );
	    return false;
	}

	p_port->context = context;

	/* Configure port */
	if (sdlc_kernel_ioctl(context, ATC_SPXS_WRITE_CONFIG, (unsigned long)&config) < 0) {
		printk( "fio_port_open: ioctl error" );
		sdlc_kernel_close(context);
		return false;
	}

	return true;
}

/*****************************************************************************/

/*****************************************************************************/
/*
This function is used to return the next scheduled timer fire for this frame.
Return of 0 indicates remove from list.
*/
/*****************************************************************************/

static int fio_hz_table[FIO_HZ_MAX] =
{
	0,		/* FIO_HZ_0 - Not Scheduled */
	0,		/* FIO_HZ_ONCE */
	1,		/* FIO_HZ_1 */
	2,		/* FIO_HZ_2 */
	5,		/* FIO_HZ_5 */
	10,		/* FIO_HZ_10 */
	20,		/* FIO_HZ_20 */
	30,		/* FIO_HZ_30 */
	40,		/* FIO_HZ_40 */
	50,		/* FIO_HZ_50 */
	60,		/* FIO_HZ_60 */
	70,		/* FIO_HZ_70 */
	80,		/* FIO_HZ_80 */
	90,		/* FIO_HZ_90 */
	100		/* FIO_HZ_100 */
};

int fiomsg_get_hertz(FIO_HZ freq)
{
	return fio_hz_table[freq];
}

FIOMSG_TIME
fiomsg_tx_frame_when
(
	FIO_HZ	freq,   /* Frequency of frame */
        bool align      /* Align "when time" at next even multiple of period */
)
{
	FIOMSG_TIME new_when;		/* New timer expiration time */
        FIOMSG_TIME now = FIOMSG_CURRENT_TIME;
        unsigned long hz = fio_hz_table[freq];

	if ( ( FIO_HZ_MAX <= freq ) || ( FIO_HZ_ONCE >= freq ) )
	{
		new_when = now; /* Handle pathological case */
	}
	else
	{
		new_when = FIOMSG_TIME_ADD(now, (FIOMSG_CLOCKS_PER_SEC / hz));
                if (align) {
                        FIOMSG_TIME_ALIGN(new_when, hz);
                        pr_debug("fiomsg_tx_frame_when: now=%lu period=%lu align=%lu\n",
                                FIOMSG_TIME_TO_NSECS(now), FIOMSG_CLOCKS_PER_SEC/hz, FIOMSG_TIME_TO_NSECS(new_when));
                }
	}

	return ( new_when );			/* Return new timer expiration time */
}

/*****************************************************************************/

/*****************************************************************************/
/*
This function is used to update a frame payload that is about to be sent.
*/
/*****************************************************************************/

void
fiomsg_tx_update_frame
(
	FIOMSG_TX_FRAME		*p_tx_frame	/* Request to ready for xmit */
)
{
	/* See if there is an update call back */
	if ( NULL != p_tx_frame->tx_func )
	{
		/* YES, then update the frame accordingly */
		( *p_tx_frame->tx_func )( p_tx_frame );
	}
}

void fiomsg_tx_notify( FIOMSG_TX_FRAME *frame )
{
        FIO_NOTIFY_INFO notify_info;
        struct fasync_struct *fa = frame->notify_async_queue;
        FIOMAN_PRIV_DATA *priv;

        notify_info.rx_frame = FIOMSG_PAYLOAD( frame )->frame_no;
        notify_info.status = FIO_FRAME_RECEIVED;
        notify_info.seq_number = 0; /* frame->seqno */
        notify_info.count = frame->len;
        
        /* Add notify info to fifo for each member of queue */
        while (fa) {
		priv = (FIOMAN_PRIV_DATA *)fa->fa_file->private_data;
                FIOMAN_FIFO_PUT(priv->frame_notification_fifo, &notify_info, sizeof(FIO_NOTIFY_INFO));
                fa = fa->fa_next;
        }
        /* Signal the queue */
        kill_fasync(&frame->notify_async_queue, SIGIO, POLL_OUT);

        /* TBD:!!!Remove queue entry if one-shot */
}

/*****************************************************************************/

/*****************************************************************************/
/*
This function is used to actually send a request frame.
*/
/*****************************************************************************/

/* TEG */
/*static	int cnt = 0;*/
/* TEG */
void
fiomsg_tx_send_frame
(
	FIOMSG_PORT	*p_port,	/* Port frame is to be sent on */
	FIOMSG_TX_FRAME	*p_tx_frame	/* Frame to be sent */
)
{
	int status;

	pr_debug("tx_send_frame(%lu) #%d, freq=%d len=%d: %x %x %x\n",
		FIOMSG_TIME_TO_NSECS(FIOMSG_CURRENT_TIME), p_tx_frame->frame[2], p_tx_frame->cur_freq, p_tx_frame->len,
		p_tx_frame->frame[0], p_tx_frame->frame[1], p_tx_frame->frame[2]);
	if( (status = sdlc_kernel_write(p_port->context, FIOMSG_PAYLOAD(p_tx_frame), p_tx_frame->len)) < 0 )
		printk( KERN_ALERT "write error %d", status );
	/* TEG */
	/*FIOMSG_FRAME	*p_payload = FIOMSG_PAYLOAD( p_tx_frame );
	printk( KERN_ALERT "Frame (%d, %d) sending, jiffies(%lu)\n", p_payload->frame_no, cnt++, FIOMSG_CURRENT_TIME );*/
	/* TEG */
}

/*****************************************************************************/

/*****************************************************************************/
/*

This is the actual FIOMSG TX Task.  This task is asynchronously run when
the kernel request frame timer fires.

The input "arg" is actually a pointer to a FIOMSG_PORT structure for the port
whose timer has fired.  There are at most FIO_PORTS_MAX FIO kernel TX timers
active at any point in time.

The FIOMSG TX Task awakes periodically (by kernel timer) to send the next
waiting request frame designated for this port.
If a response frame is expected, the RX timer for this port is set to poll for
the response. 
There is always a waiting request frame if the timer is active.

*/
/*****************************************************************************/
fiomsg_timer_callback_rtn fiomsg_tx_task( fiomsg_timer_callback_arg arg )
{
	FIOMSG_PORT			*p_port;
	FIOMSG_TX_FRAME		*p_tx_frame;	/* Next request frame in queue */
	FIOMSG_TIME		current_when;	/* Time associated with current frame */

	/* Awoken, timer has fired */

	p_port = container_of((FIOMSG_TIMER *)arg, FIOMSG_PORT, tx_timer);
	/* TEG - Make sure communications are enabled at this port */

	/* Lock resources */
	/* TEG - DO NOT LOCK SEMAPHORES, MUST USE SPINLOCKS */

	/* Get the next request frame to be sent on this port */
	if ( list_empty( &p_port->tx_queue ) )
	{
		/* No work to do, queue is empty */
		pr_debug( KERN_ALERT "TX queue is empty, no work\n" );
		/* Unlock resources */
		/* TEG - TODO */
		FIOMSG_TIMER_CALLBACK_RTN;
	}
	p_tx_frame = list_entry( p_port->tx_queue.next, FIOMSG_TX_FRAME, elem );

	/* Save current when for future reference */
	current_when = p_tx_frame->when;

	/* If request frame "when" time is at or before the current time */
	if ( FIOMSG_TIME_BEFORE_EQ( current_when, FIOMSG_CURRENT_TIME ) )
	{
		/* Time to send this frame */
		/* Figure out when this frame should be sent again */
		/*p_tx_frame->when = fiomsg_tx_frame_when( p_tx_frame->cur_freq );*/
		if (p_tx_frame->cur_freq > FIO_HZ_ONCE) {
                        p_tx_frame->when = FIOMSG_TIME_ADD(p_tx_frame->when,
                                (FIOMSG_CLOCKS_PER_SEC / fio_hz_table[ p_tx_frame->cur_freq ]));
                }

		/* Set the TX timer for next TX frame */
		fiomsg_tx_set_port_timer( p_port, p_tx_frame );

		/* Update any info in frame, such as frame 66 / 9
		   date, time stamp */
		fiomsg_tx_update_frame( p_tx_frame );

		/* Send the request frame */
		fiomsg_tx_send_frame( p_port, p_tx_frame );

		/* Set up response timer, if response is expected */
		fiomsg_rx_set_port_timer( p_port, p_tx_frame );

		/* Remove frame from queue */
		list_del_init( p_port->tx_queue.next );

		/* Requeue frame if needed */
		if (p_tx_frame->cur_freq > FIO_HZ_ONCE) {
			/* Requeue frame into correct order */
			fiomsg_tx_add_frame( p_port, p_tx_frame );
		} else {
			/* Free memory used by one time frame */
			kfree( p_tx_frame );
		}
	}
	else
	{
		/* This should never happen on a correctly running system */
		/* When a TX timer fires, there should always be pending work */
		/* to perform.  The TX timer has been reset for the next */
		/* request frame at this point, for next time. */
		/* So even if we get here, the system will keep running. */
		/* JMG: change to reset the tx timer to the current when time */
		fiomsg_tx_set_timer( p_port, current_when );
		pr_debug( KERN_ALERT "tx frame when(%d) (%lu) after jiffies (%lu)\n",
				FIOMSG_PAYLOAD(p_tx_frame)->frame_no, 
                                FIOMSG_TIME_TO_NSECS(current_when), 
                                FIOMSG_TIME_TO_NSECS(FIOMSG_CURRENT_TIME) );
	}

	/* Unlock resources */
	/* TEG - DO NOT LOCK SEMAPHORES, MUST USE SPINLOCKS */
	FIOMSG_TIMER_CALLBACK_RTN;
}

/*****************************************************************************/

/*****************************************************************************/
/*
This function is used to interface to the SDLC and actually read a RX frame.

A return value of true indicates a frame was read, otherwise false is returned.
*/
/*****************************************************************************/

int
fiomsg_rx_read_frame
(
	FIOMSG_PORT	*p_port		/* Port to read from */
)
{
	int len = 0;

	if ((len = sdlc_kernel_read(p_port->context, p_port->rx_buffer, sizeof(p_port->rx_buffer))) <= 0)
		return 0;
	
	return (len);		/* Show read */
}

/*****************************************************************************/

/*****************************************************************************/
/*

This is the actual FIOMSG RX Task.  This task is asynchronously run when
the kernel response frame timer fires.

The input "arg" is actually a pointer to a FIOMSG_PORT structure for the port
whose timer has fired.  There are at most FIO_PORTS_MAX FIO kernel RX timers
active at any point in time.

The FIOMSG RX Task awakes periodically (by kernel timer) to poll for a response
frame to a sent request frame; that a response should be received for.

*/
/*****************************************************************************/
fiomsg_timer_callback_rtn fiomsg_rx_task( fiomsg_timer_callback_arg arg )
{
						/* Port being worked on */
	FIOMSG_PORT		*p_port;
	FIOMSG_RX_PENDING	*p_rx_pend;	/* Which RX Buffer to use */
	int			frames_read = 0;
	int                     len = 0;
	/* Awoken, timer has fired */

	/* Lock resources */
	/* TEG - DO NOT LOCK SEMAPHORES, MUST USE SPINLOCKS */

	/* Get Pointer to RX Pend buffer to use */
	p_rx_pend = container_of((FIOMSG_TIMER *)arg, FIOMSG_RX_PENDING, rx_timer);
	p_port = FIOMSG_P_PORT(p_rx_pend->fiod.port);
	p_rx_pend = &p_port->rx_pend[ p_port->rx_use_pend ];
	p_port->rx_use_pend = 1 - p_port->rx_use_pend;

	/* TEG DEL */
	/*pr_debug( KERN_ALERT "RX frame task! jiffies(%lu); pend: fiod(%d), frame_no(%d)\n",
			FIOMSG_CURRENT_TIME, p_rx_pend->fiod, p_rx_pend->frame_no );*/
	/* TEG DEL */

	/* Read frame if present */
	while ( (len = fiomsg_rx_read_frame(p_port)) )
	{
		/* We read a frame.  Make sure it is the frame we are expecting */
		FIOMSG_FRAME	*rx_frame = (FIOMSG_FRAME *)(p_port->rx_buffer);

		/* Is this the frame we were expecting */
		if ( rx_frame->frame_no == p_rx_pend->frame_no )
		{
			/* We got the frame we wanted */
			p_rx_pend->frame_len = len;
			/*pr_debug( KERN_ALERT "Got RX frame(%llu) #%d\n", FIOMSG_CURRENT_TIME.tv64, rx_frame->frame_no);*/
			/* Now copy into the appropriate RX frame list */
			fiomsg_rx_update_frame( p_port, p_rx_pend, true );
			/* Update rx success count */
			FIOMSG_TIMER_CALLBACK_RTN;
		}
		/* Not the frame we expected, therefore ignore this frame */
		pr_debug( KERN_ALERT "Dumping RX frame(%lu) #%d, expected #%d\n", FIOMSG_TIME_TO_NSECS(FIOMSG_CURRENT_TIME),
				rx_frame->frame_no, p_rx_pend->frame_no);
		frames_read++;
	}
	if( frames_read == 0 ) {
		/* No frame to read, show no response */
		pr_debug( KERN_ALERT "No RX frame read!(%lu), expected #%d\n", FIOMSG_TIME_TO_NSECS(FIOMSG_CURRENT_TIME),
				p_rx_pend->frame_no);
		/* Update rx error count */
		fiomsg_rx_update_frame( p_port, p_rx_pend, false );
	}
	/* Unlock resources */
	/* TEG - DO NOT LOCK SEMAPHORES, MUST USE SPINLOCKS */
	FIOMSG_TIMER_CALLBACK_RTN;
}

/*****************************************************************************/

/*****************************************************************************/
/*
This function closes a previously opened ATC port.
*/
/*****************************************************************************/

void
fiomsg_port_close
(
	FIOMSG_PORT		*p_port	/* Pointer to port info */
)
{
	/* Clean up */
		/* Close port */
	sdlc_kernel_close(p_port->context);
		/* Clean up thread */
}

/*****************************************************************************/

/*****************************************************************************/
/*
This function initializes the FIOMSG.
*/
/*****************************************************************************/

void
fiomsg_init
(
	void
)
{
	int			ii;		/* loop variable */
	int			jj;		/* loop variable */
	FIOMSG_PORT		*p_port;	/* Port to work on */

	/* Initialize each port */
	for ( ii = 0; ii < FIO_PORTS_MAX; ii++ )
	{
		p_port = FIOMSG_P_PORT( ii );

		/* Initialize APP mutex */
		sema_init( &p_port->sema, 1 );

		p_port->port = ii;
		p_port->context = NULL;

		/* No comm to port */
		p_port->comm_enabled = 0;

		/* Init RX Pend buffer */
		p_port->tx_use_pend = 0;	/* Use RX Pend[0] to start */
		p_port->rx_use_pend = 0;	/* Use RX Pend[0] to start */

		/* Initialize timers for future use */
		FIOMSG_TIMER_INIT(&p_port->tx_timer);
		for ( jj = 0; jj < FIOMSG_RX_PEND_MAX; jj++ )
		{
			FIOMSG_TIMER_INIT( &p_port->rx_pend[jj].rx_timer );
		}

		/* Init frame queues */
		INIT_LIST_HEAD( &p_port->tx_queue );

		/* FIOD comm not enabled on port */
		for ( jj = FIO_UNDEF; jj < FIOD_MAX; jj++ )
		{
			INIT_LIST_HEAD( &p_port->rx_fiod_list[jj] );
		}
                
                init_waitqueue_head(&p_port->read_wait);

		/* Open the port */
		/*if ( ( p_port->port_opened = fiomsg_port_open( p_port ) ) )
		{
			printk( KERN_ALERT "FIOMSG Task port(%d) -> opened\n",
					fio_port[ii] );
		}*/
	}
}

/*****************************************************************************/

/*****************************************************************************/
/*
This function is used to clean up FIOMSG prior to driver unload.
*/
/*****************************************************************************/

void
fiomsg_exit
(
	void
)
{
	int			ii;			/* loop variable */
	FIOMSG_PORT	*p_port;	/* for port access */

	/* Stop, in reverse order, all FIOMSG Task */
	for ( ii = FIO_PORTS_MAX - 1; ii >= 0; ii-- )
	{
		struct list_head	*p_elem;	/* Element from FIOMAN FIOD list */
		struct list_head	*p_next;	/* Temp for loop */
		FIOMSG_TX_FRAME		*tx_frame;	/* For clean up */
		FIOMSG_RX_FRAME		*rx_frame;	/* For clean up */
		int					jj;			/* Loop variable */

		p_port = FIOMSG_P_PORT( ii );

		/* Close any ports that had been opened */
		if ( p_port->port_opened )
		{
			fiomsg_port_stop( p_port );
			fiomsg_port_close( p_port );
			p_port->port_opened = false;
			printk( KERN_ALERT
					"FIOMSG Task port(%d) -> closed\n",
					fio_port[ii] );
		}

		/* Clean up the FIOMSG lists -- should be empty (this is safety) */
		list_for_each_safe( p_elem, p_next, &p_port->tx_queue )
		{
			/* Get a ptr to this list entry */
			tx_frame = list_entry( p_elem, FIOMSG_TX_FRAME, elem );
	
			/* Delete this entry */
			list_del_init( p_elem );

			/* Free the memory */
			kfree( tx_frame );
		}

		for ( jj = 0; jj < FIOD_MAX; jj++ )
		{
			list_for_each_safe( p_elem, p_next, &p_port->rx_fiod_list[ jj ] )
			{
				/* Get a ptr to this list entry */
				rx_frame = list_entry( p_elem, FIOMSG_RX_FRAME, elem );
	
				/* Delete this entry */
				list_del_init( p_elem );

				/* Free the memory */
				kfree( rx_frame );
			}
		}
	}
}

/*****************************************************************************/

/*****************************************************************************/
/*

REVISION HISTORY:
$Log$

*/
/*****************************************************************************/
