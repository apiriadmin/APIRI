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

FIOFRAME Code Module.

*/
/*****************************************************************************/

/*  Include section.
-----------------------------------------------------------------------------*/
/* System includes. */
#include	<linux/errno.h>		/* Errno Definitions */
#include	<linux/err.h>
#include	<linux/slab.h>		/* Memory Definitions */
#include	<linux/time.h>
#include	<asm/uaccess.h>		/* User Space Access Definitions */
#include        <linux/version.h>
/* Local includes. */
#include	"fiomsg.h"
#include	"fioframe.h"

/*  Definition section.
-----------------------------------------------------------------------------*/
extern struct list_head	fioman_fiod_list;
extern int fiomsg_get_hertz(FIO_HZ freq);
extern FIOMSG_TIME fiomsg_tx_frame_when(FIO_HZ freq, bool align);
extern int local_time_offset;
extern FIOMSG_PORT	fio_port_table[ FIO_PORTS_MAX ];

/*  Global section.
-----------------------------------------------------------------------------*/

/* Frame 49: Request Module Status */
static FIOMSG_TX_FRAME frame_49_init =
{
	.def_freq	= FIO_HZ_10,
	.cur_freq	= FIO_HZ_10,
	.tx_func	= fioman_tx_frame_49,
	.resp		= true,
	.len		= FIOMAN_FRAME_NO_49_SIZE
};

/* Frame 51: Configure Input Filtering */
static FIOMSG_TX_FRAME frame_51_init =
{
	.def_freq	= FIO_HZ_ONCE,
	.cur_freq	= FIO_HZ_ONCE,
	.tx_func	= fioman_tx_frame_51,
	.resp		= true
};

/* Frame 52: Poll Raw Input Data */
static FIOMSG_TX_FRAME frame_52_init =
{
	.def_freq	= FIO_HZ_10,
	.cur_freq	= FIO_HZ_10,
	.tx_func	= fioman_tx_frame_52,
	.resp		= true,
	.len		= FIOMAN_FRAME_NO_52_SIZE
};

/* Frame 53: Poll Filtered Input Data */
static FIOMSG_TX_FRAME frame_53_init =
{
	.def_freq	= FIO_HZ_10,
	.cur_freq	= FIO_HZ_10,
	.tx_func	= fioman_tx_frame_53,
	.resp		= true,
	.len		= FIOMAN_FRAME_NO_53_SIZE
};

/* Frame 54: Poll Filtered Input Data */
static FIOMSG_TX_FRAME frame_54_init =
{
	.def_freq	= FIO_HZ_10,
	.cur_freq	= FIO_HZ_10,
	.tx_func	= fioman_tx_frame_54,
	.resp		= true,
	.len		= FIOMAN_FRAME_NO_54_SIZE
};

/* Frame 55: Set Outputs Command */
static FIOMSG_TX_FRAME frame_55_init =
{
	.def_freq	= FIO_HZ_10,
	.cur_freq	= FIO_HZ_10,
	.tx_func	= fioman_tx_frame_55,
	.resp		= true,
	.len		= FIOMAN_FRAME_NO_55_SIZE
};

/* Frame 60: Module Identification */
static FIOMSG_TX_FRAME frame_60_init =
{
	.def_freq	= FIO_HZ_ONCE,
	.cur_freq	= FIO_HZ_ONCE,
	.resp		= true,
	.len		= FIOMAN_FRAME_NO_60_SIZE
};

/* Frame 61: Switch Pack Drivers/CMU Status Request */
static FIOMSG_TX_FRAME frame_61_init =
{
	.def_freq	= FIO_HZ_10,
	.cur_freq	= FIO_HZ_10,
	.tx_func	= fioman_tx_frame_61,
	.resp		= true,
	.len		= FIOMAN_FRAME_NO_61_SIZE
};

/* Frame 62: Set Outputs Command */
static FIOMSG_TX_FRAME frame_62_init =
{
	.def_freq	= FIO_HZ_10,
	.cur_freq	= FIO_HZ_10,
	.tx_func	= fioman_tx_frame_62,
	.resp		= true,
	.len		= FIOMAN_FRAME_NO_62_SIZE
};

/* Frame 66: Time and Date Command */
static FIOMSG_TX_FRAME frame_66_init =
{
	.fiod		= {.fiod = FIOD_ALL},
	.def_freq	= FIO_HZ_1,
	.cur_freq	= FIO_HZ_1,
	.tx_func	= fioman_tx_frame_66,
	.resp		= false,
	.len		= FIOMAN_FRAME_NO_66_SIZE
};

/* Frame 67: Switch Pack Drivers/Short Status Request */
static FIOMSG_TX_FRAME frame_67_init =
{
	.def_freq	= FIO_HZ_10,
	.cur_freq	= FIO_HZ_10,
	.tx_func	= fioman_tx_frame_67,
	.resp		= true,
	.len		= FIOMAN_FRAME_NO_67_SIZE
};

/* Frame 9: Time and Date Command */
static FIOMSG_TX_FRAME frame_9_init =
{
	.fiod		= {.fiod = FIOD_ALL},
	.def_freq	= FIO_HZ_1,
	.cur_freq	= FIO_HZ_1,
	.tx_func	= fioman_tx_frame_9,
	.resp		= false,
	.len		= FIOMAN_FRAME_NO_9_SIZE
};

/* Frame 0: Load switch drivers */
static FIOMSG_TX_FRAME frame_0_init =
{
	.def_freq	= FIO_HZ_10,
	.cur_freq	= FIO_HZ_10,
	.tx_func	= fioman_tx_frame_0,
	.resp		= true,
	.len		= FIOMAN_FRAME_NO_0_SIZE
};

/* Frame 1: MMU Inputs/Status Request */
static FIOMSG_TX_FRAME frame_1_init =
{
	.def_freq	= FIO_HZ_10,
	.cur_freq	= FIO_HZ_10,
	.resp		= true,
	.len		= FIOMAN_FRAME_NO_1_SIZE
};

/* Frame 3: MMU Programming Request */
static FIOMSG_TX_FRAME frame_3_init =
{
	.def_freq	= FIO_HZ_1,
	.cur_freq	= FIO_HZ_1,
	.resp		= true,
	.len		= FIOMAN_FRAME_NO_3_SIZE
};

/* Frame 10/11: Set Outputs/Poll Inputs TFBIU 1&2 */
static FIOMSG_TX_FRAME frame_10_11_init =
{
	.def_freq	= FIO_HZ_10,
	.cur_freq	= FIO_HZ_10,
	.tx_func	= fioman_tx_frame_10_11,
	.resp		= true,
	.len		= FIOMAN_FRAME_NO_10_SIZE
};

/* Frame 12/13: Set Outputs/Poll Inputs TFBIU 3&4 */
static FIOMSG_TX_FRAME frame_12_13_init =
{
	.def_freq	= FIO_HZ_10,
	.cur_freq	= FIO_HZ_10,
	.tx_func	= fioman_tx_frame_12_13,
	.resp		= true,
	.len		= FIOMAN_FRAME_NO_12_SIZE
};

/* Frame 18: Outputs transfer frame */
static FIOMSG_TX_FRAME frame_18_init =
{
	.fiod		= {.fiod = FIOD_ALL},
	.def_freq	= FIO_HZ_10,
	.cur_freq	= FIO_HZ_10,
	.resp		= false,
	.len		= FIOMAN_FRAME_NO_18_SIZE
};

/* Frame 20_23: BIU Call Data Request*/
static FIOMSG_TX_FRAME frame_20_23_init =
{
	.def_freq	= FIO_HZ_10,
	.cur_freq	= FIO_HZ_10,
	.resp		= true,
	.len		= FIOMAN_FRAME_NO_20_SIZE
};

/* Frame 24_27: BIU Reset/Diagnostic Request */
static FIOMSG_TX_FRAME frame_24_27_init =
{
	.def_freq	= FIO_HZ_1,
	.cur_freq	= FIO_HZ_1,
	.resp		= true,
	.len		= FIOMAN_FRAME_NO_24_SIZE
};

/*  Private API declaration (prototype) section.
-----------------------------------------------------------------------------*/


/*  Public API interface section.
-----------------------------------------------------------------------------*/


/*  Private API implementation section.
-----------------------------------------------------------------------------*/
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,32)
/** The time_to_tm conversion utility, backported from later Linux kernel **/
struct tm {
	/*
	 * the number of seconds after the minute, normally in the range
	 * 0 to 59, but can be up to 60 to allow for leap seconds
	 */
	int tm_sec;
	/* the number of minutes after the hour, in the range 0 to 59*/
	int tm_min;
	/* the number of hours past midnight, in the range 0 to 23 */
	int tm_hour;
	/* the day of the month, in the range 1 to 31 */
	int tm_mday;
	/* the number of months since January, in the range 0 to 11 */
	int tm_mon;
	/* the number of years since 1900 */
	long tm_year;
	/* the number of days since Sunday, in the range 0 to 6 */
	int tm_wday;
	/* the number of days since January 1, in the range 0 to 365 */
	int tm_yday;
};
/*
 * Nonzero if YEAR is a leap year (every 4 years,
 * except every 100th isn't, and every 400th is).
 */
static int __isleap(long year)
{
	return (year) % 4 == 0 && ((year) % 100 != 0 || (year) % 400 == 0);
}

/* do a mathdiv for long type */
static long math_div(long a, long b)
{
	return a / b - (a % b < 0);
}

/* How many leap years between y1 and y2, y1 must less or equal to y2 */
static long leaps_between(long y1, long y2)
{
	long leaps1 = math_div(y1 - 1, 4) - math_div(y1 - 1, 100)
		+ math_div(y1 - 1, 400);
	long leaps2 = math_div(y2 - 1, 4) - math_div(y2 - 1, 100)
		+ math_div(y2 - 1, 400);
	return leaps2 - leaps1;
}

/* How many days come before each month (0-12). */
static const unsigned short __mon_yday[2][13] = {
	/* Normal years. */
	{0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334, 365},
	/* Leap years. */
	{0, 31, 60, 91, 121, 152, 182, 213, 244, 274, 305, 335, 366}
};

#define SECS_PER_HOUR	(60 * 60)
#define SECS_PER_DAY	(SECS_PER_HOUR * 24)

/**
 * time_to_tm - converts the calendar time to local broken-down time
 *
 * @totalsecs	the number of seconds elapsed since 00:00:00 on January 1, 1970,
 *		Coordinated Universal Time (UTC).
 * @offset	offset seconds adding to totalsecs.
 * @result	pointer to struct tm variable to receive broken-down time
 */
void time_to_tm(time_t totalsecs, int offset, struct tm *result)
{
	long days, rem, y;
	const unsigned short *ip;

	days = totalsecs / SECS_PER_DAY;
	rem = totalsecs % SECS_PER_DAY;
	rem += offset;
	while (rem < 0) {
		rem += SECS_PER_DAY;
		--days;
	}
	while (rem >= SECS_PER_DAY) {
		rem -= SECS_PER_DAY;
		++days;
	}

	result->tm_hour = rem / SECS_PER_HOUR;
	rem %= SECS_PER_HOUR;
	result->tm_min = rem / 60;
	result->tm_sec = rem % 60;

	/* January 1, 1970 was a Thursday. */
	result->tm_wday = (4 + days) % 7;
	if (result->tm_wday < 0)
		result->tm_wday += 7;

	y = 1970;

	while (days < 0 || days >= (__isleap(y) ? 366 : 365)) {
		/* Guess a corrected year, assuming 365 days per year. */
		long yg = y + math_div(days, 365);

		/* Adjust DAYS and Y to match the guessed year. */
		days -= (yg - y) * 365 + leaps_between(y, yg);
		y = yg;
	}

	result->tm_year = y - 1900;

	result->tm_yday = days;

	ip = __mon_yday[__isleap(y)];
	for (y = 11; days < ip[y]; y--)
		continue;
	days -= ip[y];

	result->tm_mon = y;
	result->tm_mday = days + 1;
}
#endif

u8 device_to_addr( FIO_DEVICE_TYPE device_type )
{
	u8	frame_addr = 20;	/* Default for 2070-2A, 2070-8, 2070-2N */

	if ((device_type >= FIOINSIU1) && (device_type <= FIOINSIU5))
		/* Input SIU */
		frame_addr = (device_type - FIOINSIU1) + 9;
	else if ( (device_type >= FIOOUT6SIU1) && (device_type <= FIOOUT6SIU4) )
		/* 6 Pack Output SIU */
		frame_addr = ((device_type - FIOOUT6SIU1 +1) % 4) + 4;
	else if ( device_type == FIOOUT14SIU1 )
		/* 14 Pack Output SIU1 */
		frame_addr = 1;
	else if ( device_type == FIOOUT14SIU2 )
		/* 14 Pack Output SIU2 */
		frame_addr = 3;
	else if ( device_type == FIOCMU )
		/* ITS CMU */
		frame_addr = 15;
	else if ( (device_type >= FIODR1) && (device_type <= FIODR8) )
		/* TS2 Detector BIU */
		frame_addr = (device_type - FIODR1) + 8;
	else if ( (device_type >= FIOTF1) && (device_type <= FIOTF8) )
		/* TS2 TF BIU */
		frame_addr = (device_type - FIOTF1);
	else if ( device_type == FIOMMU )
		/* TS2 MMU */
		frame_addr = 16;

	return frame_addr;
}
/*****************************************************************************/

/*****************************************************************************/
/*
This function is used to ready a generic request frame
*/
/*****************************************************************************/
void *
fioman_ready_generic_tx_frame
(
	FIOMAN_SYS_FIOD	*p_sys_fiod,		/* FIOD of destined frame */
	int frame_no,
	unsigned char __user *payload,
	unsigned int count
)
{
	FIOMSG_TX_FRAME	*p_tx;		/* Ptr of frame buffer */

	/* kalloc the actual frame for this port */
	/* -1 is for the one byte of frame payload defined in FIOMSG_TX_FRAME */
	if ( ( p_tx = (FIOMSG_TX_FRAME *)kzalloc( sizeof( FIOMSG_TX_FRAME ) - 1
			+ count + 3, GFP_KERNEL ) ) )
	{
		/* kmalloc succeeded, therefore init buffer */
		p_sys_fiod->frame_frequency_table[frame_no] = p_tx->cur_freq
			= p_tx->def_freq = FIO_HZ_ONCE;
		p_tx->resp = true;
		p_tx->len = count + 3;
		FIOMSG_PAYLOAD( p_tx )->frame_addr = device_to_addr(p_sys_fiod->fiod.fiod);
		FIOMSG_PAYLOAD( p_tx )->frame_ctrl = 0x83;
		FIOMSG_PAYLOAD( p_tx )->frame_no = frame_no;
		INIT_LIST_HEAD( &p_tx->elem );
		p_tx->when = FIOMSG_CURRENT_TIME;		/* Set when to send frame */
		p_tx->fioman_context = (void *)p_sys_fiod;
		p_tx->fiod = p_sys_fiod->fiod;
		copy_from_user(FIOMSG_PAYLOAD(p_tx)->frame_info, payload, count);
	}
	else
	{
		/* Could not kalloc */
		return ( ERR_PTR( -ENOMEM ) );
	}

	/* Return what we prepared */
	return ( (void *)p_tx );
}

/*****************************************************************************/
/*
This function is used to ready request frame 49 to be placed into the
request frame queue for transmission, for one port
*/
/*****************************************************************************/
void *
fioman_ready_frame_49
(
	FIOMAN_SYS_FIOD	*p_sys_fiod		/* FIOD of destined frame */
)
{
	FIOMSG_TX_FRAME	*p_tx;		/* Ptr of frame buffer */

	/* kalloc the actual frame 49 for this port */
	/* -1 is for the one byte of frame payload defined in FIOMSG_TX_FRAME */
	if ( ( p_tx = (FIOMSG_TX_FRAME *)kmalloc( sizeof( FIOMSG_TX_FRAME ) - 1
			+ FIOMAN_FRAME_NO_49_SIZE, GFP_KERNEL ) ) )
	{
		/* kmalloc succeeded, therefore init buffer */
		memcpy( p_tx, &frame_49_init, sizeof( frame_49_init ) );
		FIOMSG_PAYLOAD( p_tx )->frame_addr = device_to_addr(p_sys_fiod->fiod.fiod);
		FIOMSG_PAYLOAD( p_tx )->frame_ctrl = 0x83;
		FIOMSG_PAYLOAD( p_tx )->frame_no = FIOMAN_FRAME_NO_49;
		INIT_LIST_HEAD( &p_tx->elem );
		p_tx->when = FIOMSG_CURRENT_TIME;		/* Set when to send frame */
		p_tx->fioman_context = (void *)p_sys_fiod;
		p_tx->fiod = p_sys_fiod->fiod;
                if (p_sys_fiod->frame_frequency_table[FIOMAN_FRAME_NO_49] == -1)
		p_sys_fiod->frame_frequency_table[FIOMAN_FRAME_NO_49] = p_tx->cur_freq;
	}
	else
	{
		/* Could not kalloc */
		return ( ERR_PTR( -ENOMEM ) );
	}

	/* Return what we prepared */
	return ( (void *)p_tx );
}

/*****************************************************************************/
/*****************************************************************************/
/*
This function is called when a frame 49 is to be TX'ed.
*/
/*****************************************************************************/

void
fioman_tx_frame_49
(
	FIOMSG_TX_FRAME		*p_tx_frame		/* Frame about to be sent */
)
{
	FIOMAN_SYS_FIOD		*p_sys_fiod;	/* For access to System info */
/* TEG DEL */
/*pr_debug( "UPDATING Frame 49\n" );*/
/* TEG DEL */
	/* Copy status reset bits */
	/* Get access to system info */
	p_sys_fiod = (FIOMAN_SYS_FIOD *)p_tx_frame->fioman_context;
	/* Copy data from FIOD System buffers */
	FIOMSG_PAYLOAD( p_tx_frame )->frame_info[0] = p_sys_fiod->status_reset;

}

/*****************************************************************************/
/*****************************************************************************/
/*
This function is used to ready request frame 51 to be placed into the
request frame queue for transmission, for one port
*/
/*****************************************************************************/
void *
fioman_ready_frame_51
(
	FIOMAN_SYS_FIOD	*p_sys_fiod		/* FIOD of destined frame */
)
{
	FIOMSG_TX_FRAME	*p_tx;		/* Ptr of frame buffer */
	u16	tx_len = FIOMAN_FRAME_NO_51_SIZE;
	FIO_DEVICE_TYPE	device_type = p_sys_fiod->fiod.fiod;

	/* Default is 2070-2A, exceptions follow */
	if ((device_type == FIOTS1) || (device_type == FIOTS2))
		/* 2070-8 or 2070-2N */
		tx_len = FIOMAN_FRAME_NO_51_SIZE + 168;

	/* kalloc the actual frame 55 for this port */
	/* -1 is for the one byte of frame payload defined in FIOMSG_TX_FRAME */
	if ( ( p_tx = (FIOMSG_TX_FRAME *)kmalloc( sizeof( FIOMSG_TX_FRAME ) - 1 + tx_len,
		GFP_KERNEL ) ) )
	{
		/* kmalloc succeeded, therefore init buffer */
		memcpy( p_tx, &frame_51_init, sizeof( frame_51_init ) );
		FIOMSG_PAYLOAD( p_tx )->frame_addr = device_to_addr(p_sys_fiod->fiod.fiod);
		FIOMSG_PAYLOAD( p_tx )->frame_ctrl = 0x83;
		FIOMSG_PAYLOAD( p_tx )->frame_no = FIOMAN_FRAME_NO_51;

		INIT_LIST_HEAD( &p_tx->elem );
		p_tx->when = FIOMSG_CURRENT_TIME;		/* Set when to send frame */
		p_tx->fioman_context = (void *)p_sys_fiod;
		p_tx->fiod = p_sys_fiod->fiod;
		p_tx->len = tx_len;
                if (p_sys_fiod->frame_frequency_table[FIOMAN_FRAME_NO_51] == -1)
		p_sys_fiod->frame_frequency_table[FIOMAN_FRAME_NO_51] = p_tx->cur_freq;
		/*pr_debug("frame %d ready(%llu), when=%llu, freq=%d\n", FIOMAN_FRAME_NO_51, FIOMSG_CURRENT_TIME.tv64,
				p_tx->when.tv64, p_tx->cur_freq);*/
	}
	else
	{
		/* Could not kalloc */
		return ( ERR_PTR( -ENOMEM ) );
	}

	/* Return what we prepared */
	return ( (void *)p_tx );
}

/*****************************************************************************/
/*****************************************************************************/
/*
This function is called when a frame 51 is to be TX'ed.
*/
/*****************************************************************************/

void
fioman_tx_frame_51
(
	FIOMSG_TX_FRAME		*p_tx_frame		/* Frame about to be sent */
)
{
	FIOMAN_SYS_FIOD		*p_sys_fiod;	/* For access to System info */
	int i, count;
	unsigned long flags;
/* TEG DEL */
/*printk( KERN_ALERT "UPDATING Frame 51\n" );*/
/* TEG DEL */

	/* Copy input filter values */
	p_sys_fiod = (FIOMAN_SYS_FIOD *)p_tx_frame->fioman_context;
	spin_lock_irqsave(&p_sys_fiod->lock, flags);
	/* Copy data from FIOD System buffers */
	count = (p_tx_frame->len - 4) / 3;
	FIOMSG_PAYLOAD(p_tx_frame)->frame_info[0] = count;
	for(i=0;i<count;i++) {
		unsigned char input = i;
		if (!FIO_BIT_TEST(p_sys_fiod->input_transition_map, i))
			input |= 0x80;
		FIOMSG_PAYLOAD(p_tx_frame)->frame_info[(3*i)+1] = input;
		FIOMSG_PAYLOAD(p_tx_frame)->frame_info[(3*i)+2] = p_sys_fiod->input_filters_leading[i];
		FIOMSG_PAYLOAD(p_tx_frame)->frame_info[(3*i)+3] = p_sys_fiod->input_filters_leading[i];
	}
	spin_unlock_irqrestore(&p_sys_fiod->lock, flags);
	pr_debug( "UPDATING Frame 51: %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x\n",
		p_tx_frame->frame[3], p_tx_frame->frame[4], p_tx_frame->frame[5],
		p_tx_frame->frame[6], p_tx_frame->frame[7], p_tx_frame->frame[8],
		p_tx_frame->frame[9], p_tx_frame->frame[10], p_tx_frame->frame[11],
		p_tx_frame->frame[12], p_tx_frame->frame[13], p_tx_frame->frame[14],
		p_tx_frame->frame[15], p_tx_frame->frame[16], p_tx_frame->frame[17],
		p_tx_frame->frame[18] );
}

/*****************************************************************************/
/*****************************************************************************/
/*
This function is used to ready request frame 52 to be placed into the
request frame queue for transmission, for one port
*/
/*****************************************************************************/

void *
fioman_ready_frame_52
(
	FIOMAN_SYS_FIOD	*p_sys_fiod		/* FIOD of destined frame */
)
{
	FIOMSG_TX_FRAME	*p_tx;		/* Ptr of frame buffer */

	/* kalloc the actual frame 52 for this port */
	/* -1 is for the one byte of frame payload defined in FIOMSG_TX_FRAME */
	if ( ( p_tx = (FIOMSG_TX_FRAME *)kmalloc( sizeof( FIOMSG_TX_FRAME ) - 1
			+ FIOMAN_FRAME_NO_52_SIZE, GFP_KERNEL ) ) )
	{
		/* kmalloc succeeded, therefore init buffer */
		memcpy( p_tx, &frame_52_init, sizeof( frame_52_init ) );
		FIOMSG_PAYLOAD( p_tx )->frame_addr = device_to_addr(p_sys_fiod->fiod.fiod);
		FIOMSG_PAYLOAD( p_tx )->frame_ctrl = 0x83;
		FIOMSG_PAYLOAD( p_tx )->frame_no = FIOMAN_FRAME_NO_52;

		INIT_LIST_HEAD( &p_tx->elem );
		p_tx->when = FIOMSG_CURRENT_TIME;		/* Set when to send frame */
		p_tx->fioman_context = (void *)p_sys_fiod;
		p_tx->fiod = p_sys_fiod->fiod;
                if (p_sys_fiod->frame_frequency_table[FIOMAN_FRAME_NO_52] == -1)
		p_sys_fiod->frame_frequency_table[FIOMAN_FRAME_NO_52] = p_tx->cur_freq;
	}
	else
	{
		/* Could not kalloc */
		return ( ERR_PTR( -ENOMEM ) );
	}

	/* Return what we prepared */
	return ( (void *)p_tx );
}

/*****************************************************************************/

/*****************************************************************************/
/*
This function is called when a frame 52 is to be TX'ed.
*/
/*****************************************************************************/

void
fioman_tx_frame_52
(
	FIOMSG_TX_FRAME		*p_tx_frame		/* Frame about to be sent */
)
{
/* TEG DEL */
/*pr_debug( "UPDATING Frame 52\n" );*/
/* TEG DEL */
}

/*****************************************************************************/

/*****************************************************************************/
/*
This function is used to ready request frame 53 to be placed into the
request frame queue for transmission, for one port
*/
/*****************************************************************************/
void *
fioman_ready_frame_53
(
	FIOMAN_SYS_FIOD	*p_sys_fiod		/* FIOD of destined frame */
)
{
	FIOMSG_TX_FRAME	*p_tx;		/* Ptr of frame buffer */

	/* kalloc the actual frame 53 for this port */
	/* -1 is for the one byte of frame payload defined in FIOMSG_TX_FRAME */
	if ( ( p_tx = (FIOMSG_TX_FRAME *)kmalloc( sizeof( FIOMSG_TX_FRAME ) - 1
			+ FIOMAN_FRAME_NO_53_SIZE, GFP_KERNEL ) ) )
	{
		/* kmalloc succeeded, therefore init buffer */
		memcpy( p_tx, &frame_53_init, sizeof( frame_53_init ) );
		FIOMSG_PAYLOAD( p_tx )->frame_addr = device_to_addr(p_sys_fiod->fiod.fiod);
		FIOMSG_PAYLOAD( p_tx )->frame_ctrl = 0x83;
		FIOMSG_PAYLOAD( p_tx )->frame_no = FIOMAN_FRAME_NO_53;

		INIT_LIST_HEAD( &p_tx->elem );
		p_tx->when = FIOMSG_CURRENT_TIME;		/* Set when to send frame */
		p_tx->fioman_context = (void *)p_sys_fiod;
		p_tx->fiod = p_sys_fiod->fiod;
                if (p_sys_fiod->frame_frequency_table[FIOMAN_FRAME_NO_53] == -1)
		p_sys_fiod->frame_frequency_table[FIOMAN_FRAME_NO_53] = p_tx->cur_freq;
	}
	else
	{
		/* Could not kalloc */
		return ( ERR_PTR( -ENOMEM ) );
	}

	/* Return what we prepared */
	return ( (void *)p_tx );
}

/*****************************************************************************/

/*****************************************************************************/
/*
This function is called when a frame 53 is to be TX'ed.
*/
/*****************************************************************************/

void
fioman_tx_frame_53
(
	FIOMSG_TX_FRAME		*p_tx_frame		/* Frame about to be sent */
)
{
/* TEG DEL */
/*pr_debug( "UPDATING Frame 53\n" );*/
/* TEG DEL */
}

/*****************************************************************************/

/*****************************************************************************/
/*
This function is used to ready request frame 54 to be placed into the
request frame queue for transmission, for one port
*/
/*****************************************************************************/
void *
fioman_ready_frame_54
(
	FIOMAN_SYS_FIOD	*p_sys_fiod		/* FIOD of destined frame */
)
{
	FIOMSG_TX_FRAME	*p_tx;		/* Ptr of frame buffer */

	/* kalloc the actual frame 54 for this port */
	/* -1 is for the one byte of frame payload defined in FIOMSG_TX_FRAME */
	if ( ( p_tx = (FIOMSG_TX_FRAME *)kmalloc( sizeof( FIOMSG_TX_FRAME ) - 1
			+ FIOMAN_FRAME_NO_54_SIZE, GFP_KERNEL ) ) )
	{
		/* kmalloc succeeded, therefore init buffer */
		memcpy( p_tx, &frame_54_init, sizeof( frame_54_init ) );
		FIOMSG_PAYLOAD( p_tx )->frame_addr = device_to_addr(p_sys_fiod->fiod.fiod);
		FIOMSG_PAYLOAD( p_tx )->frame_ctrl = 0x83;
		FIOMSG_PAYLOAD( p_tx )->frame_no = FIOMAN_FRAME_NO_54;

		INIT_LIST_HEAD( &p_tx->elem );
		p_tx->when = FIOMSG_CURRENT_TIME;		/* Set when to send frame */
		p_tx->fioman_context = (void *)p_sys_fiod;
		p_tx->fiod = p_sys_fiod->fiod;
                if (p_sys_fiod->frame_frequency_table[FIOMAN_FRAME_NO_54] == -1)
                        p_sys_fiod->frame_frequency_table[FIOMAN_FRAME_NO_54] = p_tx->cur_freq;
	}
	else
	{
		/* Could not kalloc */
		return ( ERR_PTR( -ENOMEM ) );
	}

	/* Return what we prepared */
	return ( (void *)p_tx );
}

/*****************************************************************************/

/*****************************************************************************/
/*
This function is called when a frame 53 is to be TX'ed.
*/
/*****************************************************************************/

void
fioman_tx_frame_54
(
	FIOMSG_TX_FRAME		*p_tx_frame		/* Frame about to be sent */
)
{
/* TEG DEL */
pr_debug( "UPDATING Frame 54\n" );
/* TEG DEL */
        FIOMSG_PAYLOAD(p_tx_frame)->frame_info[0]++;
        
}

/*****************************************************************************/
/*****************************************************************************/
/*
This function is used to ready request frame 55 to be placed into the
request frame queue for transmission, for one port
*/
/*****************************************************************************/

void *
fioman_ready_frame_55
(
	FIOMAN_SYS_FIOD	*p_sys_fiod		/* FIOD of destined frame */
)
{
	FIOMSG_TX_FRAME	*p_tx;		/* Ptr of frame buffer */
	u16	tx_len = FIOMAN_FRAME_NO_55_SIZE;
	FIO_DEVICE_TYPE	device_type = p_sys_fiod->fiod.fiod;

	/* Default is 2070-2A, exceptions follow */
	if ((device_type == FIOTS1) || (device_type == FIOTS2))
		/* 2070-8 or 2070-2N */
		tx_len = FIOMAN_FRAME_NO_55_SIZE + 10;

	/* kalloc the actual frame 55 for this port */
	/* -1 is for the one byte of frame payload defined in FIOMSG_TX_FRAME */
	if ( ( p_tx = (FIOMSG_TX_FRAME *)kmalloc( sizeof( FIOMSG_TX_FRAME ) - 1 + tx_len,
		GFP_KERNEL ) ) )
	{
		/* kmalloc succeeded, therefore init buffer */
		memcpy( p_tx, &frame_55_init, sizeof( frame_55_init ) );
		FIOMSG_PAYLOAD( p_tx )->frame_addr = device_to_addr(p_sys_fiod->fiod.fiod);
		FIOMSG_PAYLOAD( p_tx )->frame_ctrl = 0x83;
		FIOMSG_PAYLOAD( p_tx )->frame_no = FIOMAN_FRAME_NO_55;

		INIT_LIST_HEAD( &p_tx->elem );
		p_tx->when = FIOMSG_CURRENT_TIME;		/* Set when to send frame */
		p_tx->fioman_context = (void *)p_sys_fiod;
		p_tx->fiod = p_sys_fiod->fiod;
		p_tx->len = tx_len;
                if (p_sys_fiod->frame_frequency_table[FIOMAN_FRAME_NO_55] == -1)
		p_sys_fiod->frame_frequency_table[FIOMAN_FRAME_NO_55] = p_tx->cur_freq;
	}
	else
	{
		/* Could not kalloc */
		return ( ERR_PTR( -ENOMEM ) );
	}

	/* Return what we prepared */
	return ( (void *)p_tx );
}

/*****************************************************************************/

/*****************************************************************************/
/*
This function is called when a frame 55 is to be TX'ed.
*/
/*****************************************************************************/

void
fioman_tx_frame_55
(
	FIOMSG_TX_FRAME		*p_tx_frame		/* Frame about to be sent */
)
{
	FIOMAN_SYS_FIOD		*p_sys_fiod;	/* For access to System info */
	int i, count;
	unsigned long	flags;
/* TEG DEL */
/*pr_debug( "UPDATING Frame 55:\n" );*/
/* copy output data and ctrl bitmaps to outgoing frame, translate mapping */

	/* Get access to system info */
	p_sys_fiod = (FIOMAN_SYS_FIOD *)p_tx_frame->fioman_context;

	spin_lock_irqsave(&p_sys_fiod->lock, flags);
	/* Copy data from FIOD System buffers */
	count = (p_tx_frame->len-3)/2;
	for(i=0;i<count;i++) {
		if (FIOMSG_PAYLOAD(p_tx_frame)->frame_info[i] != p_sys_fiod->outputs_plus[i]) {
			FIOMSG_PAYLOAD(p_tx_frame)->frame_info[i] = p_sys_fiod->outputs_plus[i];
		}
		if (FIOMSG_PAYLOAD(p_tx_frame)->frame_info[i+count] !=
				(p_sys_fiod->outputs_minus[i] ^ p_sys_fiod->outputs_plus[i])) {
			FIOMSG_PAYLOAD(p_tx_frame)->frame_info[i+count] =
				(p_sys_fiod->outputs_minus[i] ^ p_sys_fiod->outputs_plus[i]);
		}
	}

	/* For TS1 & TS2 set the Fault Monitor output */
	if (p_sys_fiod->fiod.fiod == FIOTS1 || p_sys_fiod->fiod.fiod == FIOTS2) {
		FIO_BIT_CLEAR(&FIOMSG_PAYLOAD(p_tx_frame)->frame_info[count], 78);
		if (p_sys_fiod->fm_state == FIO_TS_FM_OFF)
			FIO_BIT_SET(FIOMSG_PAYLOAD(p_tx_frame)->frame_info, 78);
		else
			FIO_BIT_CLEAR(FIOMSG_PAYLOAD(p_tx_frame)->frame_info, 78);
		/* For TS1 set the Volt Monitor output */
		if (p_sys_fiod->fiod.fiod == FIOTS1) {
			FIO_BIT_CLEAR(&FIOMSG_PAYLOAD(p_tx_frame)->frame_info[count], 79);
			if (p_sys_fiod->vm_state == FIO_TS1_VM_OFF)
				FIO_BIT_SET(FIOMSG_PAYLOAD(p_tx_frame)->frame_info, 79);
			else
				FIO_BIT_CLEAR(FIOMSG_PAYLOAD(p_tx_frame)->frame_info, 79);
		}
	}
#ifdef NEW_WATCHDOG
        if ((p_sys_fiod->watchdog_output >= 0)
                && (p_sys_fiod->watchdog_rate > FIO_HZ_ONCE) 
                && (p_tx_frame->cur_freq > FIO_HZ_ONCE)) {
                if (p_sys_fiod->watchdog_countdown == 0) {
                        /* toggle output */
                        p_sys_fiod->watchdog_state = !p_sys_fiod->watchdog_state;
                        /* refresh count */
                        p_sys_fiod->watchdog_countdown = 
                                fiomsg_get_hertz(p_tx_frame->cur_freq) / fiomsg_get_hertz(p_sys_fiod->watchdog_rate);
                }
                p_sys_fiod->watchdog_countdown--;
                FIO_BIT_CLEAR(&FIOMSG_PAYLOAD(p_tx_frame)->frame_info[count], p_sys_fiod->watchdog_output);
                if (p_sys_fiod->watchdog_state)
                        FIO_BIT_SET(FIOMSG_PAYLOAD(p_tx_frame)->frame_info, p_sys_fiod->watchdog_output);
                else
                        FIO_BIT_CLEAR(FIOMSG_PAYLOAD(p_tx_frame)->frame_info, p_sys_fiod->watchdog_output);
        }
#else
	/* Clear the trigger condition now that the watchdog output is sent */
	p_sys_fiod->watchdog_trigger_condition = false;
#endif
	spin_unlock_irqrestore(&p_sys_fiod->lock, flags);
	pr_debug( "UPDATING Frame 55: %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x\n",
		p_tx_frame->frame[3], p_tx_frame->frame[4], p_tx_frame->frame[5],
		p_tx_frame->frame[6], p_tx_frame->frame[7], p_tx_frame->frame[8],
		p_tx_frame->frame[9], p_tx_frame->frame[10], p_tx_frame->frame[11],
		p_tx_frame->frame[12], p_tx_frame->frame[13], p_tx_frame->frame[14],
		p_tx_frame->frame[15], p_tx_frame->frame[16], p_tx_frame->frame[17],
		p_tx_frame->frame[18] );

}

/*****************************************************************************/

/*****************************************************************************/
/*
This function is used to ready request frame 60 to be placed into the
request frame queue for transmission, for one port
*/
/*****************************************************************************/

void *
fioman_ready_frame_60
(
	FIOMAN_SYS_FIOD	*p_sys_fiod		/* FIOD of destined frame */
)
{
	FIOMSG_TX_FRAME	*p_tx;		/* Ptr of frame buffer */

	/* kalloc the actual frame 60 for this port */
	/* -1 is for the one byte of frame payload defined in FIOMSG_TX_FRAME */
	if ( ( p_tx = (FIOMSG_TX_FRAME *)kmalloc( sizeof( FIOMSG_TX_FRAME ) - 1
			+ FIOMAN_FRAME_NO_60_SIZE, GFP_KERNEL ) ) )
	{
		/* kmalloc succeeded, therefore init buffer */
		memcpy( p_tx, &frame_60_init, sizeof( frame_60_init ) );
		FIOMSG_PAYLOAD( p_tx )->frame_addr = device_to_addr(p_sys_fiod->fiod.fiod);
		FIOMSG_PAYLOAD( p_tx )->frame_ctrl = 0x83;
		FIOMSG_PAYLOAD( p_tx )->frame_no = FIOMAN_FRAME_NO_60;

		INIT_LIST_HEAD( &p_tx->elem );
		p_tx->when = FIOMSG_CURRENT_TIME;		/* Set when to send frame */
		p_tx->fioman_context = (void *)p_sys_fiod;
		p_tx->fiod = p_sys_fiod->fiod;
                if (p_sys_fiod->frame_frequency_table[FIOMAN_FRAME_NO_60] == -1)
		p_sys_fiod->frame_frequency_table[FIOMAN_FRAME_NO_60] = p_tx->cur_freq;
	}
	else
	{
		/* Could not kalloc */
		return ( ERR_PTR( -ENOMEM ) );
	}

	/* Return what we prepared */
	return ( (void *)p_tx );
}

/*****************************************************************************/

/*****************************************************************************/
/*
This function is used to ready request frame 61 to be placed into the
request frame queue for transmission, for one port
*/
/*****************************************************************************/

void *
fioman_ready_frame_61
(
	FIOMAN_SYS_FIOD	*p_sys_fiod		/* FIOD of destined frame */
)
{
	FIOMSG_TX_FRAME	*p_tx;		/* Ptr of frame buffer */

	/* kalloc the actual frame 61 for this port */
	/* -1 is for the one byte of frame payload defined in FIOMSG_TX_FRAME */
	if ( ( p_tx = (FIOMSG_TX_FRAME *)kmalloc( sizeof( FIOMSG_TX_FRAME ) - 1	+ FIOMAN_FRAME_NO_61_SIZE, GFP_KERNEL ) ) )
	{
		/* kmalloc succeeded, therefore init buffer */
		memcpy( p_tx, &frame_61_init, sizeof( frame_61_init ) );
		FIOMSG_PAYLOAD( p_tx )->frame_addr = 15;
		FIOMSG_PAYLOAD( p_tx )->frame_ctrl = 0x83;
		FIOMSG_PAYLOAD( p_tx )->frame_no = FIOMAN_FRAME_NO_61;
		INIT_LIST_HEAD( &p_tx->elem );
		p_tx->when = FIOMSG_CURRENT_TIME;		/* Set when to send frame */
		p_tx->fioman_context = (void *)p_sys_fiod;
		p_tx->fiod = p_sys_fiod->fiod;
                if (p_sys_fiod->frame_frequency_table[FIOMAN_FRAME_NO_61] == -1)
			p_sys_fiod->frame_frequency_table[FIOMAN_FRAME_NO_61] = p_tx->cur_freq;
	}
	else
	{
		/* Could not kalloc */
		return ( ERR_PTR( -ENOMEM ) );
	}

	/* Return what we prepared */
	return ( (void *)p_tx );
}

/*****************************************************************************/
/*
This function is called when a frame 61 is to be TX'ed.
*/
/*****************************************************************************/

void
fioman_tx_frame_61
(
	FIOMSG_TX_FRAME		*p_tx_frame		/* Frame about to be sent */
)
{
	struct list_head	*p_sys_elem;	/* Element from FIOMAN FIOD list */
	struct list_head	*p_next;		/* Temp for loop */
	FIOMAN_SYS_FIOD		*p_sys_fiod;	/* FIOD of destined frame */
	FIO_DEVICE_TYPE		fiod;
	int ii, output;
	unsigned long flags;
/* TEG DEL */
/*pr_debug( "UPDATING Frame 61\n" );*/
/* TEG DEL */
	/* Fill in payload with channel status */
	memset(FIOMSG_PAYLOAD(p_tx_frame)->frame_info, 0, (FIOMAN_FRAME_NO_61_SIZE-3));
	list_for_each_safe( p_sys_elem, p_next, &fioman_fiod_list )
	{
		/* Get a ptr to this list entry */
		p_sys_fiod = list_entry( p_sys_elem, FIOMAN_SYS_FIOD, elem );
		for (fiod=FIOOUT6SIU1;fiod<=FIOOUT14SIU2;fiod++) {
			if (p_sys_fiod->fiod.fiod == fiod) {
				/* Add channel states for this device */
				spin_lock_irqsave(&p_sys_fiod->lock, flags);
				for(ii=0; ii<FIO_CHANNELS; ii++) {
					if ( (output = p_sys_fiod->channel_map_green[ii]) > 0 ) {
						if (FIO_BIT_TEST(p_sys_fiod->outputs_plus, (output-1)))
							FIOMSG_PAYLOAD(p_tx_frame)->frame_info[8+ii/8] |= (0x1<<(ii%8));
					}
					if ( (output = p_sys_fiod->channel_map_yellow[ii]) > 0 ) {
						if (FIO_BIT_TEST(p_sys_fiod->outputs_plus, (output-1)))
							FIOMSG_PAYLOAD(p_tx_frame)->frame_info[4+ii/8] |= (0x1<<(ii%8));
					}
					if ( (output = p_sys_fiod->channel_map_red[ii]) > 0 ) {
						if (FIO_BIT_TEST(p_sys_fiod->outputs_plus, (output-1)))
							FIOMSG_PAYLOAD(p_tx_frame)->frame_info[ii/8] |= (0x1<<(ii%8));
					}
				}
				spin_unlock_irqrestore(&p_sys_fiod->lock, flags);
			}
		}
	}
	/* Add Dark Channel Map Selection */
	p_sys_fiod = (FIOMAN_SYS_FIOD *)p_tx_frame->fioman_context;
	spin_lock_irqsave(&p_sys_fiod->lock, flags);
	FIOMSG_PAYLOAD(p_tx_frame)->frame_info[12] = p_sys_fiod->cmu_mask;
	spin_unlock_irqrestore(&p_sys_fiod->lock, flags);
/*	printk( KERN_ALERT "UPDATING Frame 61: %x %x %x %x %x %x %x %x %x %x %x %x %x\n",
			p_tx_frame->frame[3], p_tx_frame->frame[4], p_tx_frame->frame[5],
			p_tx_frame->frame[6], p_tx_frame->frame[7], p_tx_frame->frame[8],
			p_tx_frame->frame[9], p_tx_frame->frame[10], p_tx_frame->frame[11],
			p_tx_frame->frame[12], p_tx_frame->frame[13], p_tx_frame->frame[14],
			p_tx_frame->frame[15] );*/

}

/*****************************************************************************/
/*
This function is used to ready request frame 62 to be placed into the
request frame queue for transmission, for one port
*/
/*****************************************************************************/

void *
fioman_ready_frame_62
(
	FIOMAN_SYS_FIOD	*p_sys_fiod		/* FIOD of destined frame */
)
{
	FIOMSG_TX_FRAME	*p_tx;		/* Ptr of frame buffer */

	/* kalloc the actual frame 62 for this port */
	/* -1 is for the one byte of frame payload defined in FIOMSG_TX_FRAME */
	if ( ( p_tx = (FIOMSG_TX_FRAME *)kmalloc( sizeof( FIOMSG_TX_FRAME ) - 1 + FIOMAN_FRAME_NO_62_SIZE,
		GFP_KERNEL ) ) )
	{
		/* kmalloc succeeded, therefore init buffer */
		memcpy( p_tx, &frame_62_init, sizeof( frame_62_init ) );
		FIOMSG_PAYLOAD( p_tx )->frame_addr = 15;
		FIOMSG_PAYLOAD( p_tx )->frame_ctrl = 0x83;
		FIOMSG_PAYLOAD( p_tx )->frame_no = FIOMAN_FRAME_NO_62;

		INIT_LIST_HEAD( &p_tx->elem );
		p_tx->when = FIOMSG_CURRENT_TIME;		/* Set when to send frame */
		p_tx->fioman_context = (void *)p_sys_fiod;
		p_tx->fiod = p_sys_fiod->fiod;
                if (p_sys_fiod->frame_frequency_table[FIOMAN_FRAME_NO_62] == -1)
			p_sys_fiod->frame_frequency_table[FIOMAN_FRAME_NO_62] = p_tx->cur_freq;
	}
	else
	{
		/* Could not kalloc */
		return ( ERR_PTR( -ENOMEM ) );
	}

	/* Return what we prepared */
	return ( (void *)p_tx );
}

/*****************************************************************************/

/*****************************************************************************/
/*
This function is called when a frame 62 is to be TX'ed.
*/
/*****************************************************************************/

void
fioman_tx_frame_62
(
	FIOMSG_TX_FRAME		*p_tx_frame		/* Frame about to be sent */
)
{
	FIOMAN_SYS_FIOD		*p_sys_fiod;	/* For access to System info */
	unsigned long	flags;

	/* Get access to system info */
	p_sys_fiod = (FIOMAN_SYS_FIOD *)p_tx_frame->fioman_context;

	spin_lock_irqsave(&p_sys_fiod->lock, flags);
	/* Copy data from FIOD System setting */
	FIOMSG_PAYLOAD(p_tx_frame)->frame_info[0] = p_sys_fiod->cmu_fsa;
	spin_unlock_irqrestore(&p_sys_fiod->lock, flags);
}

/*****************************************************************************/
/*****************************************************************************/
/*
This function is used to ready request frame 66 to be placed into the
request frame queue for transmission, for one port
*/
/*****************************************************************************/

void *
fioman_ready_frame_66
(
	FIOMAN_SYS_FIOD	*p_sys_fiod		/* FIOD of destined frame */
)
{
	FIOMSG_TX_FRAME	*p_tx;		/* Ptr of frame buffer */

	/* kalloc the actual frame 66 for this port */
	/* -1 is for the one byte of frame payload defined in FIOMSG_TX_FRAME */
	if ( ( p_tx = (FIOMSG_TX_FRAME *)kmalloc( sizeof( FIOMSG_TX_FRAME ) - 1
											+ FIOMAN_FRAME_NO_66_SIZE,
											GFP_KERNEL ) ) )
	{
		/* kmalloc succeeded, therefore init buffer */
		memcpy( p_tx, &frame_66_init, sizeof( frame_66_init ) );
		FIOMSG_PAYLOAD( p_tx )->frame_addr = FIOMSG_ADDR_ALL;
		FIOMSG_PAYLOAD( p_tx )->frame_ctrl = 0x83;
		FIOMSG_PAYLOAD( p_tx )->frame_no = FIOMAN_FRAME_NO_66;

		INIT_LIST_HEAD( &p_tx->elem );
		p_tx->when = FIOMSG_CURRENT_TIME;		/* Set when to send frame */
		p_tx->fiod = p_sys_fiod->fiod;
                if (p_sys_fiod->frame_frequency_table[FIOMAN_FRAME_NO_66] == -1)
		p_sys_fiod->frame_frequency_table[FIOMAN_FRAME_NO_66] = p_tx->cur_freq;
	}
	else
	{
		/* Could not kalloc */
		return ( ERR_PTR( -ENOMEM ) );
	}

	/* Return what we prepared */
	return ( (void *)p_tx );
}

/*****************************************************************************/
/*
This function is called when a frame 66 is to be TX'ed.
*/
/*****************************************************************************/

void
fioman_tx_frame_66
(
	FIOMSG_TX_FRAME		*p_tx_frame		/* Frame about to be sent */
)
{
	struct timeval tv;
	struct tm tm = {0};
	int offset = 0; /* this is time adjust from fio_set_local_time_offset() */
/* TEG DEL */
/*pr_debug( "UPDATING Frame 66\n" );*/
/* TEG DEL */
	/* fill in payload with date/time */
	do_gettimeofday(&tv);
	time_to_tm(tv.tv_sec,offset,&tm);
	FIOMSG_PAYLOAD( p_tx_frame )->frame_info[0] = tm.tm_mon+1;
	FIOMSG_PAYLOAD( p_tx_frame )->frame_info[1] = tm.tm_mday;
	FIOMSG_PAYLOAD( p_tx_frame )->frame_info[2] = tm.tm_year%100;
	FIOMSG_PAYLOAD( p_tx_frame )->frame_info[3] = tm.tm_hour;
	FIOMSG_PAYLOAD( p_tx_frame )->frame_info[4] = tm.tm_min;
	FIOMSG_PAYLOAD( p_tx_frame )->frame_info[5] = tm.tm_sec;
	FIOMSG_PAYLOAD( p_tx_frame )->frame_info[6] = tv.tv_usec/100000L; /* tenths */
}

/*****************************************************************************/
/*
This function is used to ready request frame 67 to be placed into the
request frame queue for transmission, for one port
*/
/*****************************************************************************/

void *
fioman_ready_frame_67
(
	FIOMAN_SYS_FIOD	*p_sys_fiod		/* FIOD of destined frame */
)
{
	FIOMSG_TX_FRAME	*p_tx;		/* Ptr of frame buffer */

	/* kalloc the actual frame 67 for this port */
	/* -1 is for the one byte of frame payload defined in FIOMSG_TX_FRAME */
	if ( ( p_tx = (FIOMSG_TX_FRAME *)kmalloc( sizeof( FIOMSG_TX_FRAME ) - 1	+ FIOMAN_FRAME_NO_67_SIZE, GFP_KERNEL ) ) )
	{
		/* kmalloc succeeded, therefore init buffer */
		memcpy( p_tx, &frame_67_init, sizeof( frame_67_init ) );
		FIOMSG_PAYLOAD( p_tx )->frame_addr = 15;
		FIOMSG_PAYLOAD( p_tx )->frame_ctrl = 0x83;
		FIOMSG_PAYLOAD( p_tx )->frame_no = FIOMAN_FRAME_NO_67;
		INIT_LIST_HEAD( &p_tx->elem );
		p_tx->when = FIOMSG_CURRENT_TIME;		/* Set when to send frame */
		p_tx->fioman_context = (void *)p_sys_fiod;
		p_tx->fiod = p_sys_fiod->fiod;
                if (p_sys_fiod->frame_frequency_table[FIOMAN_FRAME_NO_67] == -1)
		p_sys_fiod->frame_frequency_table[FIOMAN_FRAME_NO_67] = p_tx->cur_freq;
	}
	else
	{
		/* Could not kalloc */
		return ( ERR_PTR( -ENOMEM ) );
	}

	/* Return what we prepared */
	return ( (void *)p_tx );
}

/*****************************************************************************/
/*
This function is called when a frame 67 is to be TX'ed.
*/
/*****************************************************************************/

void
fioman_tx_frame_67
(
	FIOMSG_TX_FRAME		*p_tx_frame		/* Frame about to be sent */
)
{
	struct list_head	*p_sys_elem;	/* Element from FIOMAN FIOD list */
	struct list_head	*p_next;		/* Temp for loop */
	FIOMAN_SYS_FIOD		*p_sys_fiod;	/* FIOD of destined frame */
	FIO_DEVICE_TYPE		fiod;
	int ii, output;
	unsigned long flags;
/* TEG DEL */
/*pr_debug( "UPDATING Frame 67\n" );*/
/* TEG DEL */
	/* Fill in payload with channel status */
	memset(FIOMSG_PAYLOAD(p_tx_frame)->frame_info, 0, (FIOMAN_FRAME_NO_67_SIZE-3));
	list_for_each_safe( p_sys_elem, p_next, &fioman_fiod_list )
	{
		/* Get a ptr to this list entry */
		p_sys_fiod = list_entry( p_sys_elem, FIOMAN_SYS_FIOD, elem );
		for (fiod=FIOOUT6SIU1;fiod<=FIOOUT14SIU2;fiod++) {
			if (p_sys_fiod->fiod.fiod == fiod) {
/*				pr_debug( "Frame 0: out+: %x %x %x\n",	p_sys_fiod->outputs_plus[0],
					p_sys_fiod->outputs_plus[1], p_sys_fiod->outputs_plus[2] );
				pr_debug( "Frame 0: out-: %x %x %x\n",	p_sys_fiod->outputs_minus[0],
					p_sys_fiod->outputs_minus[1], p_sys_fiod->outputs_minus[2] );
				pr_debug( "Frame 0: grn: %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x\n",
					p_sys_fiod->channel_map_green[0], p_sys_fiod->channel_map_green[1],
					p_sys_fiod->channel_map_green[2], p_sys_fiod->channel_map_green[3],
					p_sys_fiod->channel_map_green[4], p_sys_fiod->channel_map_green[5],
					p_sys_fiod->channel_map_green[6], p_sys_fiod->channel_map_green[7],
					p_sys_fiod->channel_map_green[8], p_sys_fiod->channel_map_green[9],
					p_sys_fiod->channel_map_green[10], p_sys_fiod->channel_map_green[11],
					p_sys_fiod->channel_map_green[12], p_sys_fiod->channel_map_green[13],
					p_sys_fiod->channel_map_green[14], p_sys_fiod->channel_map_green[15]);
				pr_debug( "Frame 0: yel: %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x\n",
					p_sys_fiod->channel_map_yellow[0], p_sys_fiod->channel_map_yellow[1],
					p_sys_fiod->channel_map_yellow[2], p_sys_fiod->channel_map_yellow[3],
					p_sys_fiod->channel_map_yellow[4], p_sys_fiod->channel_map_yellow[5],
					p_sys_fiod->channel_map_yellow[6], p_sys_fiod->channel_map_yellow[7],
					p_sys_fiod->channel_map_yellow[8], p_sys_fiod->channel_map_yellow[9],
					p_sys_fiod->channel_map_yellow[10], p_sys_fiod->channel_map_yellow[11],
					p_sys_fiod->channel_map_yellow[12], p_sys_fiod->channel_map_yellow[13],
					p_sys_fiod->channel_map_yellow[14], p_sys_fiod->channel_map_yellow[15]);
				pr_debug( "Frame 0: red: %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x\n",
					p_sys_fiod->channel_map_red[0], p_sys_fiod->channel_map_red[1],
					p_sys_fiod->channel_map_red[2], p_sys_fiod->channel_map_red[3],
					p_sys_fiod->channel_map_red[4], p_sys_fiod->channel_map_red[5],
					p_sys_fiod->channel_map_red[6], p_sys_fiod->channel_map_red[7],
					p_sys_fiod->channel_map_red[8], p_sys_fiod->channel_map_red[9],
					p_sys_fiod->channel_map_red[10], p_sys_fiod->channel_map_red[11],
					p_sys_fiod->channel_map_red[12], p_sys_fiod->channel_map_red[13],
					p_sys_fiod->channel_map_red[14], p_sys_fiod->channel_map_red[15]);*/
				/* Add channel states for this device */
				spin_lock_irqsave(&p_sys_fiod->lock, flags);
				for(ii=0; ii<FIO_CHANNELS; ii++) {
					if ( (output = p_sys_fiod->channel_map_green[ii]) > 0 ) {
						if (FIO_BIT_TEST(p_sys_fiod->outputs_plus, (output-1)))
							FIOMSG_PAYLOAD(p_tx_frame)->frame_info[8+ii/8] |= (0x1<<(ii%8));
					}
					if ( (output = p_sys_fiod->channel_map_yellow[ii]) > 0 ) {
						if (FIO_BIT_TEST(p_sys_fiod->outputs_plus, (output-1)))
							FIOMSG_PAYLOAD(p_tx_frame)->frame_info[4+ii/8] |= (0x1<<(ii%8));
					}
					if ( (output = p_sys_fiod->channel_map_red[ii]) > 0 ) {
						if (FIO_BIT_TEST(p_sys_fiod->outputs_plus, (output-1)))
							FIOMSG_PAYLOAD(p_tx_frame)->frame_info[ii/8] |= (0x1<<(ii%8));
					}
				}
				spin_unlock_irqrestore(&p_sys_fiod->lock, flags);
			}
		}
	}
	/* Add Dark Channel Map Selection */
	p_sys_fiod = (FIOMAN_SYS_FIOD *)p_tx_frame->fioman_context;
	spin_lock_irqsave(&p_sys_fiod->lock, flags);
	FIOMSG_PAYLOAD(p_tx_frame)->frame_info[12] = p_sys_fiod->cmu_mask;
	spin_unlock_irqrestore(&p_sys_fiod->lock, flags);
/*	printk( KERN_ALERT "UPDATING Frame 67: %x %x %x %x %x %x %x %x %x %x %x %x %x\n",
			p_tx_frame->frame[3], p_tx_frame->frame[4], p_tx_frame->frame[5],
			p_tx_frame->frame[6], p_tx_frame->frame[7], p_tx_frame->frame[8],
			p_tx_frame->frame[9], p_tx_frame->frame[10], p_tx_frame->frame[11],
			p_tx_frame->frame[12], p_tx_frame->frame[13], p_tx_frame->frame[14],
			p_tx_frame->frame[15] );*/

}

/*****************************************************************************/
/*
This function is used to ready broadcast frame 9 to be placed into the
request frame queue for transmission, for one port
*/
/*****************************************************************************/

void *
fioman_ready_frame_9
(
	FIOMAN_SYS_FIOD	*p_sys_fiod		/* FIOD of destined frame */
)
{
	FIOMSG_TX_FRAME	*p_tx;		/* Ptr of frame buffer */

	/* kalloc the actual frame 9 for this port */
	/* -1 is for the one byte of frame payload defined in FIOMSG_TX_FRAME */
	if ( ( p_tx = (FIOMSG_TX_FRAME *)kmalloc( sizeof( FIOMSG_TX_FRAME ) - 1
                                        + FIOMAN_FRAME_NO_9_SIZE, GFP_KERNEL ) ) )
	{
		/* kmalloc succeeded, therefore init buffer */
		memcpy( p_tx, &frame_9_init, sizeof( frame_9_init ) );
		FIOMSG_PAYLOAD( p_tx )->frame_addr = FIOMSG_ADDR_ALL;
		FIOMSG_PAYLOAD( p_tx )->frame_ctrl = 0x83;
		FIOMSG_PAYLOAD( p_tx )->frame_no = FIOMAN_FRAME_NO_9;

		INIT_LIST_HEAD( &p_tx->elem );
		p_tx->when = fiomsg_tx_frame_when(p_tx->cur_freq, true); /* Set when to send first frame */
		pr_debug("frame %d ready(%lu), when=%lu\n", FIOMAN_FRAME_NO_9, FIOMSG_TIME_TO_NSECS(FIOMSG_CURRENT_TIME),
				FIOMSG_TIME_TO_NSECS(p_tx->when));
		p_tx->fiod = p_sys_fiod->fiod;
                if (p_sys_fiod->frame_frequency_table[FIOMAN_FRAME_NO_9] == -1)
		p_sys_fiod->frame_frequency_table[FIOMAN_FRAME_NO_9] = p_tx->cur_freq;
	}
	else
	{
		/* Could not kalloc */
		return ( ERR_PTR( -ENOMEM ) );
	}

	/* Return what we prepared */
	return ( (void *)p_tx );
}

/*****************************************************************************/

/*****************************************************************************/
/*
This function is called when a frame 9 is to be TX'ed.
*/
/*****************************************************************************/

void
fioman_tx_frame_9
(
	FIOMSG_TX_FRAME		*p_tx_frame		/* Frame about to be sent */
)
{
	struct timeval tv;
	struct tm tm = {0};
	struct list_head	*p_sys_elem;	/* Element from FIOMAN FIOD list */
	struct list_head	*p_next;		/* Temp for loop */
	FIOMAN_SYS_FIOD		*p_sys_fiod;	/* Device ptr */
	FIO_DEVICE_TYPE		fiod;

/* TEG DEL */
/*pr_debug( "UPDATING Frame 9\n" );*/
/* TEG DEL */
	/* fill in payload date/time fields */
	do_gettimeofday(&tv);
	time_to_tm(tv.tv_sec,local_time_offset,&tm);
	FIOMSG_PAYLOAD( p_tx_frame )->frame_info[0] = tm.tm_mon+1;
	FIOMSG_PAYLOAD( p_tx_frame )->frame_info[1] = tm.tm_mday;
	FIOMSG_PAYLOAD( p_tx_frame )->frame_info[2] = tm.tm_year%100;
	FIOMSG_PAYLOAD( p_tx_frame )->frame_info[3] = tm.tm_hour;
	FIOMSG_PAYLOAD( p_tx_frame )->frame_info[4] = tm.tm_min;
	FIOMSG_PAYLOAD( p_tx_frame )->frame_info[5] = tm.tm_sec;
	FIOMSG_PAYLOAD( p_tx_frame )->frame_info[6] = tv.tv_usec/100000L; /* tenths */
	/* fill in payload BIU presence map */
	FIOMSG_PAYLOAD( p_tx_frame )->frame_info[7] = 0;
	FIOMSG_PAYLOAD( p_tx_frame )->frame_info[8] = 0;
	list_for_each_safe( p_sys_elem, p_next, &fioman_fiod_list )
	{
		/* Get a ptr to this list entry */
		p_sys_fiod = list_entry( p_sys_elem, FIOMAN_SYS_FIOD, elem );
		for (fiod=FIOTF1;fiod<=FIOTF8;fiod++) {
			if (p_sys_fiod->fiod.fiod == fiod)
				FIOMSG_PAYLOAD(p_tx_frame)->frame_info[7] |= (1<<(fiod-FIOTF1));
		}
		for (fiod=FIODR1;fiod<=FIODR8;fiod++) {
			if (p_sys_fiod->fiod.fiod == fiod)
				FIOMSG_PAYLOAD(p_tx_frame)->frame_info[8] |= (1<<(fiod-FIODR1));
		}
	}

}

/*****************************************************************************/


/*****************************************************************************/
/*
This function is used to ready request frame 0 to be placed into the
request frame queue for transmission, for one port
*/
/*****************************************************************************/

void *
fioman_ready_frame_0
(
	FIOMAN_SYS_FIOD	*p_sys_fiod		/* FIOD of destined frame */
)
{
	FIOMSG_TX_FRAME	*p_tx;		/* Ptr of frame buffer */

	/* kalloc the actual frame for this port */
	/* -1 is for the one byte of frame payload defined in FIOMSG_TX_FRAME */
	if ( ( p_tx = (FIOMSG_TX_FRAME *)kmalloc( sizeof( FIOMSG_TX_FRAME ) - 1 + FIOMAN_FRAME_NO_0_SIZE, GFP_KERNEL ) ) )
	{
		/* kmalloc succeeded, therefore init buffer */
		memcpy( p_tx, &frame_0_init, sizeof( frame_0_init ) );
		FIOMSG_PAYLOAD( p_tx )->frame_addr = 16;
		FIOMSG_PAYLOAD( p_tx )->frame_ctrl = 0x83;
		FIOMSG_PAYLOAD( p_tx )->frame_no = FIOMAN_FRAME_NO_0;

		INIT_LIST_HEAD( &p_tx->elem );
		p_tx->when = fiomsg_tx_frame_when(p_tx->cur_freq, true);	/* Set when to send first frame */
		p_tx->fioman_context = (void *)p_sys_fiod;
		p_tx->fiod = p_sys_fiod->fiod;
		pr_debug("frame %d ready(%lu), when=%lu\n", FIOMAN_FRAME_NO_0, FIOMSG_TIME_TO_NSECS(FIOMSG_CURRENT_TIME),
				FIOMSG_TIME_TO_NSECS(p_tx->when));
                if (p_sys_fiod->frame_frequency_table[FIOMAN_FRAME_NO_0] == -1)
		p_sys_fiod->frame_frequency_table[FIOMAN_FRAME_NO_0] = p_tx->cur_freq;
	}
	else
	{
		/* Could not kalloc */
		return ( ERR_PTR( -ENOMEM ) );
	}

	/* Return what we prepared */
	return ( (void *)p_tx );
}

/*****************************************************************************/
/*
This function is called when a frame 0 is to be TX'ed.
*/
/*****************************************************************************/

void
fioman_tx_frame_0
(
	FIOMSG_TX_FRAME		*p_tx_frame		/* Frame about to be sent */
)
{
	struct list_head	*p_sys_elem;	/* Element from FIOMAN FIOD list */
	struct list_head	*p_next;		/* Temp for loop */
	FIOMAN_SYS_FIOD		*p_sys_fiod;	/* FIOD of destined frame */
	//FIO_DEVICE_TYPE		fiod;
	int ii, output;
	unsigned long flags;
/* TEG DEL */
/*pr_debug( "UPDATING Frame 0\n" );*/
/* TEG DEL */
	/* Fill in payload with channel status */
	memset(FIOMSG_PAYLOAD(p_tx_frame)->frame_info, 0, (FIOMAN_FRAME_NO_0_SIZE-3));
	list_for_each_safe( p_sys_elem, p_next, &fioman_fiod_list )
	{
		/* Get a ptr to this list entry */
		p_sys_fiod = list_entry( p_sys_elem, FIOMAN_SYS_FIOD, elem );
		if (IS_TFBIU(p_sys_fiod->fiod.fiod)	|| (p_sys_fiod->fiod.fiod == FIOTS1)) {
/*				pr_debug("Frame 0: out+: %x %x %x\n",	p_sys_fiod->outputs_plus[0],
					p_sys_fiod->outputs_plus[1], p_sys_fiod->outputs_plus[2] );
				pr_debug("Frame 0: out-: %x %x %x\n",	p_sys_fiod->outputs_minus[0],
					p_sys_fiod->outputs_minus[1], p_sys_fiod->outputs_minus[2] );
				pr_debug("Frame 0: grn: %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x\n",
					p_sys_fiod->channel_map_green[0], p_sys_fiod->channel_map_green[1],
					p_sys_fiod->channel_map_green[2], p_sys_fiod->channel_map_green[3],
					p_sys_fiod->channel_map_green[4], p_sys_fiod->channel_map_green[5],
					p_sys_fiod->channel_map_green[6], p_sys_fiod->channel_map_green[7],
					p_sys_fiod->channel_map_green[8], p_sys_fiod->channel_map_green[9],
					p_sys_fiod->channel_map_green[10], p_sys_fiod->channel_map_green[11],
					p_sys_fiod->channel_map_green[12], p_sys_fiod->channel_map_green[13],
					p_sys_fiod->channel_map_green[14], p_sys_fiod->channel_map_green[15]);
				pr_debug("Frame 0: yel: %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x\n",
					p_sys_fiod->channel_map_yellow[0], p_sys_fiod->channel_map_yellow[1],
					p_sys_fiod->channel_map_yellow[2], p_sys_fiod->channel_map_yellow[3],
					p_sys_fiod->channel_map_yellow[4], p_sys_fiod->channel_map_yellow[5],
					p_sys_fiod->channel_map_yellow[6], p_sys_fiod->channel_map_yellow[7],
					p_sys_fiod->channel_map_yellow[8], p_sys_fiod->channel_map_yellow[9],
					p_sys_fiod->channel_map_yellow[10], p_sys_fiod->channel_map_yellow[11],
					p_sys_fiod->channel_map_yellow[12], p_sys_fiod->channel_map_yellow[13],
					p_sys_fiod->channel_map_yellow[14], p_sys_fiod->channel_map_yellow[15]);
				pr_debug("Frame 0: red: %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x\n",
					p_sys_fiod->channel_map_red[0], p_sys_fiod->channel_map_red[1],
					p_sys_fiod->channel_map_red[2], p_sys_fiod->channel_map_red[3],
					p_sys_fiod->channel_map_red[4], p_sys_fiod->channel_map_red[5],
					p_sys_fiod->channel_map_red[6], p_sys_fiod->channel_map_red[7],
					p_sys_fiod->channel_map_red[8], p_sys_fiod->channel_map_red[9],
					p_sys_fiod->channel_map_red[10], p_sys_fiod->channel_map_red[11],
					p_sys_fiod->channel_map_red[12], p_sys_fiod->channel_map_red[13],
					p_sys_fiod->channel_map_red[14], p_sys_fiod->channel_map_red[15]);*/
			/* Add channel states for this device */
			spin_lock_irqsave(&p_sys_fiod->lock, flags);
			for(ii=0; ii<FIO_CHANNELS; ii++) {
				if ( (output = p_sys_fiod->channel_map_green[ii]) > 0 ) {
					if (FIO_BIT_TEST(p_sys_fiod->outputs_plus, (output-1)))
						FIOMSG_PAYLOAD(p_tx_frame)->frame_info[ii/4] |= (0x1<<(2*(ii%4)));
					if (IS_TFBIU(p_sys_fiod->fiod.fiod) && FIO_BIT_TEST(p_sys_fiod->outputs_minus, (output-1)))
						FIOMSG_PAYLOAD(p_tx_frame)->frame_info[ii/4] |= (0x2<<(2*(ii%4)));
				}
				if ( (output = p_sys_fiod->channel_map_yellow[ii]) > 0 ) {
					if (FIO_BIT_TEST(p_sys_fiod->outputs_plus, (output-1)))
						FIOMSG_PAYLOAD(p_tx_frame)->frame_info[4+ii/4] |= (0x1<<(2*(ii%4)));
					if (IS_TFBIU(p_sys_fiod->fiod.fiod) && FIO_BIT_TEST(p_sys_fiod->outputs_minus, (output-1)))
						FIOMSG_PAYLOAD(p_tx_frame)->frame_info[4+ii/4] |= (0x2<<(2*(ii%4)));
				}
				if ( (output = p_sys_fiod->channel_map_red[ii]) > 0 ) {
					if (FIO_BIT_TEST(p_sys_fiod->outputs_plus, (output-1)))
						FIOMSG_PAYLOAD(p_tx_frame)->frame_info[8+ii/4] |= (0x1<<(2*(ii%4)));
					if (IS_TFBIU(p_sys_fiod->fiod.fiod) && FIO_BIT_TEST(p_sys_fiod->outputs_minus, (output-1)))
						FIOMSG_PAYLOAD(p_tx_frame)->frame_info[8+ii/4] |= (0x2<<(2*(ii%4)));
				}
			}
			spin_unlock_irqrestore(&p_sys_fiod->lock, flags);
		}
	}
	p_sys_fiod = (FIOMAN_SYS_FIOD *)p_tx_frame->fioman_context;
	spin_lock_irqsave(&p_sys_fiod->lock, flags);
	FIOMSG_PAYLOAD(p_tx_frame)->frame_info[12] =
			(p_sys_fiod->flash_bit == FIO_MMU_FLASH_BIT_ON)?0x80:0x0;
	spin_unlock_irqrestore(&p_sys_fiod->lock, flags);
	/*pr_debug("UPDATING Frame 0: %x %x %x %x %x %x %x %x %x %x %x %x %x\n",
			p_tx_frame->frame[3], p_tx_frame->frame[4], p_tx_frame->frame[5],
			p_tx_frame->frame[6], p_tx_frame->frame[7], p_tx_frame->frame[8],
			p_tx_frame->frame[9], p_tx_frame->frame[10], p_tx_frame->frame[11],
			p_tx_frame->frame[12], p_tx_frame->frame[13], p_tx_frame->frame[14],
			p_tx_frame->frame[15] );*/
}

/*****************************************************************************/
/*****************************************************************************/
/*
This function is used to ready request frame 1 to be placed into the
request frame queue for transmission, for one port
*/
/*****************************************************************************/

void *
fioman_ready_frame_1
(
	FIOMAN_SYS_FIOD	*p_sys_fiod		/* FIOD of destined frame */
)
{
	FIOMSG_TX_FRAME	*p_tx;		/* Ptr of frame buffer */

	/* kalloc the actual frame 1 for this port */
	/* -1 is for the one byte of frame payload defined in FIOMSG_TX_FRAME */
	if ( ( p_tx = (FIOMSG_TX_FRAME *)kmalloc( sizeof( FIOMSG_TX_FRAME ) - 1
		+ FIOMAN_FRAME_NO_1_SIZE, GFP_KERNEL ) ) )
	{
		/* kmalloc succeeded, therefore init buffer */
		memcpy( p_tx, &frame_1_init, sizeof( frame_1_init ) );
		FIOMSG_PAYLOAD( p_tx )->frame_addr = 16;
		FIOMSG_PAYLOAD( p_tx )->frame_ctrl = 0x83;
		FIOMSG_PAYLOAD( p_tx )->frame_no = FIOMAN_FRAME_NO_1;

		INIT_LIST_HEAD( &p_tx->elem );
		p_tx->when = fiomsg_tx_frame_when(p_tx->cur_freq, true);	/* Set when to send first frame */
		p_tx->fioman_context = (void *)p_sys_fiod;
		p_tx->fiod = p_sys_fiod->fiod;
		pr_debug("frame %d ready(%lu), when=%lu\n", FIOMAN_FRAME_NO_1, FIOMSG_TIME_TO_NSECS(FIOMSG_CURRENT_TIME),
				FIOMSG_TIME_TO_NSECS(p_tx->when));
                if (p_sys_fiod->frame_frequency_table[FIOMAN_FRAME_NO_1] == -1)
		p_sys_fiod->frame_frequency_table[FIOMAN_FRAME_NO_1] = p_tx->cur_freq;
	}
	else
	{
		/* Could not kalloc */
		return ( ERR_PTR( -ENOMEM ) );
	}

	/* Return what we prepared */
	return ( (void *)p_tx );
}

/*****************************************************************************/
/*****************************************************************************/
/*
This function is used to ready request frame 3 to be placed into the
request frame queue for transmission, for one port
*/
/*****************************************************************************/

void *
fioman_ready_frame_3
(
	FIOMAN_SYS_FIOD	*p_sys_fiod		/* FIOD of destined frame */
)
{
	FIOMSG_TX_FRAME	*p_tx;		/* Ptr of frame buffer */

	/* kalloc the actual frame 1 for this port */
	/* -1 is for the one byte of frame payload defined in FIOMSG_TX_FRAME */
	if ( ( p_tx = (FIOMSG_TX_FRAME *)kmalloc( sizeof( FIOMSG_TX_FRAME ) - 1
		+ FIOMAN_FRAME_NO_3_SIZE, GFP_KERNEL ) ) )
	{
		/* kmalloc succeeded, therefore init buffer */
		memcpy( p_tx, &frame_3_init, sizeof( frame_3_init ) );
		FIOMSG_PAYLOAD( p_tx )->frame_addr = 16;
		FIOMSG_PAYLOAD( p_tx )->frame_ctrl = 0x83;
		FIOMSG_PAYLOAD( p_tx )->frame_no = FIOMAN_FRAME_NO_3;

		INIT_LIST_HEAD( &p_tx->elem );
		p_tx->when = fiomsg_tx_frame_when(p_tx->cur_freq, true);	/* Set when to send first frame */
		/* add window offset according to spec */
		p_tx->when = FIOMSG_TIME_ADD( p_tx->when, (FIOMSG_CLOCKS_PER_SEC / fiomsg_get_hertz(FIO_HZ_10)) );
		p_tx->fioman_context = (void *)p_sys_fiod;
		p_tx->fiod = p_sys_fiod->fiod;
		pr_debug("frame %d ready(%lu), when=%lu\n", FIOMAN_FRAME_NO_3, FIOMSG_TIME_TO_NSECS(FIOMSG_CURRENT_TIME),
				FIOMSG_TIME_TO_NSECS(p_tx->when));
                if (p_sys_fiod->frame_frequency_table[FIOMAN_FRAME_NO_3] == -1)
		p_sys_fiod->frame_frequency_table[FIOMAN_FRAME_NO_3] = p_tx->cur_freq;
	}
	else
	{
		/* Could not kalloc */
		return ( ERR_PTR( -ENOMEM ) );
	}

	/* Return what we prepared */
	return ( (void *)p_tx );
}

/*****************************************************************************/
/*****************************************************************************/
/*
This function is used to ready request frame 10/11 to be placed into the
request frame queue for transmission, for one port
*/
/*****************************************************************************/

void *
fioman_ready_frame_10_11
(
	FIOMAN_SYS_FIOD	*p_sys_fiod,		/* FIOD of destined frame */
	int frame_no
)
{
	FIOMSG_TX_FRAME	*p_tx;		/* Ptr of frame buffer */

	/* kalloc the actual frame for this port */
	/* -1 is for the one byte of frame payload defined in FIOMSG_TX_FRAME */
	if ( ( p_tx = (FIOMSG_TX_FRAME *)kmalloc( sizeof( FIOMSG_TX_FRAME ) - 1
		+ FIOMAN_FRAME_NO_10_SIZE, GFP_KERNEL ) ) )
	{
		/* kmalloc succeeded, therefore init buffer */
		memcpy( p_tx, &frame_10_11_init, sizeof( frame_10_11_init ) );
		FIOMSG_PAYLOAD( p_tx )->frame_addr = (frame_no == FIOMAN_FRAME_NO_10)?0:1;
		FIOMSG_PAYLOAD( p_tx )->frame_ctrl = 0x83;
		FIOMSG_PAYLOAD( p_tx )->frame_no = frame_no;

		INIT_LIST_HEAD( &p_tx->elem );
		p_tx->when = fiomsg_tx_frame_when(p_tx->cur_freq, true);	/* Set when to send first frame */
		p_tx->fioman_context = (void *)p_sys_fiod;
		p_tx->fiod = p_sys_fiod->fiod;
		pr_debug("frame %d ready(%lu), when=%lu\n", frame_no, FIOMSG_TIME_TO_NSECS(FIOMSG_CURRENT_TIME),
				FIOMSG_TIME_TO_NSECS(p_tx->when));
                if (p_sys_fiod->frame_frequency_table[frame_no] == -1)
		p_sys_fiod->frame_frequency_table[frame_no] = p_tx->cur_freq;
	}
	else
	{
		/* Could not kalloc */
		return ( ERR_PTR( -ENOMEM ) );
	}

	/* Return what we prepared */
	return ( (void *)p_tx );
}

/*****************************************************************************/
/*
This function is called when a frame 10/11 is to be TX'ed.
*/
/*****************************************************************************/

void
fioman_tx_frame_10_11
(
	FIOMSG_TX_FRAME		*p_tx_frame		/* Frame about to be sent */
)
{
	FIOMAN_SYS_FIOD		*p_sys_fiod;	/* For access to System info */
	int i;
	u8 src_mask, dst_mask;
	unsigned long flags;

/* copy output bitmaps to outgoing frame */

	/* Get access to system info */
	p_sys_fiod = (FIOMAN_SYS_FIOD *)p_tx_frame->fioman_context;

	/* Copy data from FIOD System buffers */
	spin_lock_irqsave(&p_sys_fiod->lock, flags);
	for(i=0;i<24;i++) {
		src_mask = 1<<(i%8);
		dst_mask = 1<<((i%4)*2);
		if (p_sys_fiod->outputs_plus[i/8] & src_mask)
			FIOMSG_PAYLOAD(p_tx_frame)->frame_info[i/4] |= dst_mask;
		else
			FIOMSG_PAYLOAD(p_tx_frame)->frame_info[i/4] &= ~dst_mask;

		if (p_sys_fiod->outputs_minus[i/8] & src_mask)
			FIOMSG_PAYLOAD(p_tx_frame)->frame_info[i/4] |= dst_mask<<1;
		else
			FIOMSG_PAYLOAD(p_tx_frame)->frame_info[i/4] &= ~(dst_mask<<1);
	}
	for(i=0;i<2;i++) {
		FIOMSG_PAYLOAD(p_tx_frame)->frame_info[6 + i] =
			p_sys_fiod->outputs_plus[3 + i] | p_sys_fiod->outputs_minus[3 + i];
	}
	/* TEG DEL */
	/*if ((p_sys_fiod->outputs_plus[0]|p_sys_fiod->outputs_plus[1]|p_sys_fiod->outputs_plus[2]) != 0xff)
		pr_debug( "UPDATING Frame 10/11: %x %x %x %x %x %x\n",
			p_tx_frame->frame[3], p_tx_frame->frame[4], p_tx_frame->frame[5],
			p_tx_frame->frame[6], p_tx_frame->frame[7], p_tx_frame->frame[8] );*/
	spin_unlock_irqrestore(&p_sys_fiod->lock, flags);

}

/*****************************************************************************/
/*****************************************************************************/
/*
This function is used to ready request frame 12/13 to be placed into the
request frame queue for transmission, for one port
*/
/*****************************************************************************/

void *
fioman_ready_frame_12_13
(
	FIOMAN_SYS_FIOD	*p_sys_fiod,		/* FIOD of destined frame */
	int frame_no
)
{
	FIOMSG_TX_FRAME	*p_tx;		/* Ptr of frame buffer */

	/* kalloc the actual frame for this port */
	/* -1 is for the one byte of frame payload defined in FIOMSG_TX_FRAME */
	if ( ( p_tx = (FIOMSG_TX_FRAME *)kmalloc( sizeof( FIOMSG_TX_FRAME ) - 1
		+ FIOMAN_FRAME_NO_12_SIZE, GFP_KERNEL ) ) )
	{
		/* kmalloc succeeded, therefore init buffer */
		memcpy( p_tx, &frame_12_13_init, sizeof( frame_12_13_init ) );
		FIOMSG_PAYLOAD( p_tx )->frame_addr = (frame_no == FIOMAN_FRAME_NO_12)?2:3;
		FIOMSG_PAYLOAD( p_tx )->frame_ctrl = 0x83;
		FIOMSG_PAYLOAD( p_tx )->frame_no = frame_no;

		INIT_LIST_HEAD( &p_tx->elem );
		p_tx->when = fiomsg_tx_frame_when(p_tx->cur_freq, true);	/* Set when to send first frame */
		p_tx->fioman_context = (void *)p_sys_fiod;
		p_tx->fiod = p_sys_fiod->fiod;
		pr_debug("frame %d ready(%lu), when=%lu\n", frame_no, FIOMSG_TIME_TO_NSECS(FIOMSG_CURRENT_TIME),
				FIOMSG_TIME_TO_NSECS(p_tx->when));
                if (p_sys_fiod->frame_frequency_table[frame_no] == -1)
		p_sys_fiod->frame_frequency_table[frame_no] = p_tx->cur_freq;
	}
	else
	{
		/* Could not kalloc */
		return ( ERR_PTR( -ENOMEM ) );
	}

	/* Return what we prepared */
	return ( (void *)p_tx );
}

/*****************************************************************************/
/*
This function is called when a frame 12/13 is to be TX'ed.
*/
/*****************************************************************************/

void
fioman_tx_frame_12_13
(
	FIOMSG_TX_FRAME		*p_tx_frame		/* Frame about to be sent */
)
{
	FIOMAN_SYS_FIOD		*p_sys_fiod;	/* For access to System info */
	int i;
	unsigned long flags;

/* copy output bitmaps to outgoing frame */

	/* Get access to system info */
	p_sys_fiod = (FIOMAN_SYS_FIOD *)p_tx_frame->fioman_context;

	/* Copy data from FIOD System buffers */
	spin_lock_irqsave(&p_sys_fiod->lock, flags);
	for(i=0;i<5;i++) {
		FIOMSG_PAYLOAD(p_tx_frame)->frame_info[i] =
			p_sys_fiod->outputs_plus[i] | p_sys_fiod->outputs_minus[i];
	}
	spin_unlock_irqrestore(&p_sys_fiod->lock, flags);
	/* TEG DEL */
	/*pr_debug("UPDATING Frame 10/11: %x %x %x %x %x %x\n",
			p_tx_frame->frame[3], p_tx_frame->frame[4], p_tx_frame->frame[5],
			p_tx_frame->frame[6], p_tx_frame->frame[7], p_tx_frame->frame[8] );*/

}

/*****************************************************************************/
/*****************************************************************************/
/*
This function is used to ready broadcast frame 18 to be placed into the
request frame queue for transmission, for one port
*/
/*****************************************************************************/

void *
fioman_ready_frame_18
(
	FIOMAN_SYS_FIOD	*p_sys_fiod		/* FIOD of destined frame */
)
{
	FIOMSG_TX_FRAME	*p_tx;		/* Ptr of frame buffer */

	/* kalloc the actual frame 18 for this port */
	/* -1 is for the one byte of frame payload defined in FIOMSG_TX_FRAME */
	if ( ( p_tx = (FIOMSG_TX_FRAME *)kmalloc( sizeof( FIOMSG_TX_FRAME ) - 1
		+ FIOMAN_FRAME_NO_18_SIZE, GFP_KERNEL ) ) )
	{
		/* kmalloc succeeded, therefore init buffer */
		memcpy( p_tx, &frame_18_init, sizeof( frame_18_init ) );
		FIOMSG_PAYLOAD( p_tx )->frame_addr = FIOMSG_ADDR_ALL;
		FIOMSG_PAYLOAD( p_tx )->frame_ctrl = 0x83;
		FIOMSG_PAYLOAD( p_tx )->frame_no = FIOMAN_FRAME_NO_18;

		INIT_LIST_HEAD( &p_tx->elem );
		p_tx->when = fiomsg_tx_frame_when(p_tx->cur_freq, true);	/* Set when to send first frame */
		pr_debug("frame %d ready(%lu), when=%lu\n", FIOMAN_FRAME_NO_18, FIOMSG_TIME_TO_NSECS(FIOMSG_CURRENT_TIME),
				FIOMSG_TIME_TO_NSECS(p_tx->when));
                if (p_sys_fiod->frame_frequency_table[FIOMAN_FRAME_NO_18] == -1)
		p_sys_fiod->frame_frequency_table[FIOMAN_FRAME_NO_18] = p_tx->cur_freq;
	}
	else
	{
		/* Could not kalloc */
		return ( ERR_PTR( -ENOMEM ) );
	}

	/* Return what we prepared */
	return ( (void *)p_tx );
}

/*****************************************************************************/
/*
This function is used to ready request frame 20 to be placed into the
request frame queue for transmission, for one port
*/
/*****************************************************************************/

void *
fioman_ready_frame_20_23
(
	FIOMAN_SYS_FIOD	*p_sys_fiod,		/* FIOD of destined frame */
	u8 frame_no
)
{
	FIOMSG_TX_FRAME	*p_tx;		/* Ptr of frame buffer */

	/* kalloc the actual frame 20 for this port */
	/* -1 is for the one byte of frame payload defined in FIOMSG_TX_FRAME */
	if ( ( p_tx = (FIOMSG_TX_FRAME *)kmalloc( sizeof( FIOMSG_TX_FRAME ) - 1
		+ FIOMAN_FRAME_NO_20_SIZE, GFP_KERNEL ) ) )
	{
		/* kmalloc succeeded, therefore init buffer */
		memcpy( p_tx, &frame_20_23_init, sizeof( frame_20_23_init ) );
		FIOMSG_PAYLOAD( p_tx )->frame_addr = 8 + (frame_no - FIOMAN_FRAME_NO_20);
		FIOMSG_PAYLOAD( p_tx )->frame_ctrl = 0x83;
		FIOMSG_PAYLOAD( p_tx )->frame_no = frame_no;

		INIT_LIST_HEAD( &p_tx->elem );
		p_tx->when = fiomsg_tx_frame_when(p_tx->cur_freq, true);	/* Set when to send first frame */
		p_tx->fioman_context = (void *)p_sys_fiod;
		p_tx->fiod = p_sys_fiod->fiod;
		pr_debug("frame %d ready(%lu), when=%lu\n", frame_no, FIOMSG_TIME_TO_NSECS(FIOMSG_CURRENT_TIME),
				FIOMSG_TIME_TO_NSECS(p_tx->when));
                if (p_sys_fiod->frame_frequency_table[frame_no] == -1)
		p_sys_fiod->frame_frequency_table[frame_no] = p_tx->cur_freq;
	}
	else
	{
		/* Could not kalloc */
		return ( ERR_PTR( -ENOMEM ) );
	}

	/* Return what we prepared */
	return ( (void *)p_tx );
}

/*****************************************************************************/
/*
This function is used to ready request frame 24 to be placed into the
request frame queue for transmission, for one port
*/
/*****************************************************************************/

void *
fioman_ready_frame_24_27
(
	FIOMAN_SYS_FIOD	*p_sys_fiod,		/* FIOD of destined frame */
	u8 frame_no
)
{
	FIOMSG_TX_FRAME	*p_tx;		/* Ptr of frame buffer */

	/* kalloc the actual frame 24 for this port */
	/* -1 is for the one byte of frame payload defined in FIOMSG_TX_FRAME */
	if ( ( p_tx = (FIOMSG_TX_FRAME *)kzalloc( sizeof( FIOMSG_TX_FRAME ) - 1
		+ FIOMAN_FRAME_NO_24_SIZE, GFP_KERNEL ) ) )
	{
		/* kmalloc succeeded, therefore init buffer */
		memcpy( p_tx, &frame_24_27_init, sizeof( frame_24_27_init ) );
		FIOMSG_PAYLOAD( p_tx )->frame_addr = 8 + (frame_no - FIOMAN_FRAME_NO_24);
		FIOMSG_PAYLOAD( p_tx )->frame_ctrl = 0x83;
		FIOMSG_PAYLOAD( p_tx )->frame_no = frame_no;

		INIT_LIST_HEAD( &p_tx->elem );
		p_tx->when = fiomsg_tx_frame_when(p_tx->cur_freq, true);	/* Set when to send first frame */
		p_tx->fioman_context = (void *)p_sys_fiod;
		p_tx->fiod = p_sys_fiod->fiod;
		pr_debug("frame %d ready(%llu), when=%llu\n", frame_no, FIOMSG_CURRENT_TIME.tv64,
				p_tx->when.tv64);
                if (p_sys_fiod->frame_frequency_table[frame_no] == -1)
                        p_sys_fiod->frame_frequency_table[frame_no] = p_tx->cur_freq;
	}
	else
	{
		/* Could not kalloc */
		return ( ERR_PTR( -ENOMEM ) );
	}

	/* Return what we prepared */
	return ( (void *)p_tx );
}

/*****************************************************************************/
/*
 * Common initialization for FIOMSG_RX_FRAME type
 */
void fioman_rx_frame_init(FIOMAN_SYS_FIOD *p_sys_fiod, FIOMSG_RX_FRAME *p_rx_frame)
{
	INIT_LIST_HEAD( &p_rx_frame->elem );
	p_rx_frame->rx_func = NULL;
	memset(&p_rx_frame->info, 0, sizeof(FIO_FRAME_INFO));
	p_rx_frame->info.frequency = FIO_HZ_10;
	p_rx_frame->resp = false;
	p_rx_frame->fioman_context = (void *)p_sys_fiod;
	p_rx_frame->fiod = p_sys_fiod->fiod.fiod;
	p_rx_frame->notify_async_queue = NULL;
}


/*****************************************************************************/
/*
This function is used to ready response frame 138/139 to be placed into the
response frame list for this fiod for this port.
*/
/*****************************************************************************/

void *
fioman_ready_generic_rx_frame
(
	FIOMAN_SYS_FIOD	*p_sys_fiod,		/* FIOD of destined frame */
	u8 frame_no
)
{
	FIOMSG_RX_FRAME	*p_rx;		/* Ptr of frame buffer */

	/* kalloc the actual frame for this port */
	/* -1 is for the one byte of frame payload defined in FIOMSG_RX_FRAME */
	if ( ( p_rx = (FIOMSG_RX_FRAME *)kzalloc( sizeof( FIOMSG_RX_FRAME ) - 1
		+ FIOMAN_MAX_RX_FRAME_SIZE, GFP_KERNEL ) ) )
	{
		/* kmalloc succeeded, therefore init buffer */
		fioman_rx_frame_init(p_sys_fiod, p_rx);
		FIOMSG_PAYLOAD( p_rx )->frame_no = frame_no;
		p_rx->len = FIOMAN_MAX_RX_FRAME_SIZE;
	}
	else
	{
		/* Could not kalloc */
		return ( ERR_PTR( -ENOMEM ) );
	}

	/* Return what we prepared */
	return ( (void *)p_rx );
}

/*****************************************************************************/
/*****************************************************************************/
/*
This function is used to ready response frame 138/139 to be placed into the
response frame list for this fiod for this port.
*/
/*****************************************************************************/

void *
fioman_ready_frame_138_141
(
	FIOMAN_SYS_FIOD	*p_sys_fiod,		/* FIOD of destined frame */
	u8 frame_no
)
{
	FIOMSG_RX_FRAME	*p_rx;		/* Ptr of frame buffer */

	/* kalloc the actual frame 138/139/140/141 for this port */
	/* -1 is for the one byte of frame payload defined in FIOMSG_RX_FRAME */
	if ( ( p_rx = (FIOMSG_RX_FRAME *)kzalloc( sizeof( FIOMSG_RX_FRAME ) - 1
		+ FIOMAN_FRAME_NO_138_SIZE, GFP_KERNEL ) ) )
	{
		/* kmalloc succeeded, therefore init buffer */
		fioman_rx_frame_init(p_sys_fiod, p_rx);
		FIOMSG_PAYLOAD( p_rx )->frame_no = frame_no;
		p_rx->rx_func = &fioman_rx_frame_138_141;
		p_rx->len = FIOMAN_FRAME_NO_138_SIZE;
	}
	else
	{
		/* Could not kalloc */
		return ( ERR_PTR( -ENOMEM ) );
	}

	/* Return what we prepared */
	return ( (void *)p_rx );
}

/*****************************************************************************/
/*****************************************************************************/
/*
This function is used to ready response frame 148/151 to be placed into the
response frame list for this fiod for this port.
*/
/*****************************************************************************/

void *
fioman_ready_frame_148_151
(
	FIOMAN_SYS_FIOD	*p_sys_fiod,		/* FIOD of destined frame */
	u8 frame_no
)
{
	FIOMSG_RX_FRAME	*p_rx;		/* Ptr of frame buffer */

	/* kalloc the actual frame 148/149/150/151 for this port */
	/* -1 is for the one byte of frame payload defined in FIOMSG_RX_FRAME */
	if ( ( p_rx = (FIOMSG_RX_FRAME *)kzalloc( sizeof( FIOMSG_RX_FRAME ) - 1
		  + FIOMAN_FRAME_NO_148_SIZE, GFP_KERNEL ) ) )
	{
		/* kmalloc succeeded, therefore init buffer */
		fioman_rx_frame_init(p_sys_fiod, p_rx);
		FIOMSG_PAYLOAD( p_rx )->frame_no = frame_no;
		p_rx->rx_func = &fioman_rx_frame_148_151;
		p_rx->len = FIOMAN_FRAME_NO_148_SIZE;
	}
	else
	{
		/* Could not kalloc */
		return ( ERR_PTR( -ENOMEM ) );
	}

	/* Return what we prepared */
	return ( (void *)p_rx );
}

/*****************************************************************************/
/*
This function is used to ready response frame 152/155 to be placed into the
response frame list for this fiod for this port.
*/
/*****************************************************************************/

void *
fioman_ready_frame_152_155
(
	FIOMAN_SYS_FIOD	*p_sys_fiod,		/* FIOD of destined frame */
	u8 frame_no
)
{
	FIOMSG_RX_FRAME	*p_rx;		/* Ptr of frame buffer */

	/* kalloc the actual frame 152/153/154/155 for this port */
	/* -1 is for the one byte of frame payload defined in FIOMSG_RX_FRAME */
	if ( ( p_rx = (FIOMSG_RX_FRAME *)kzalloc( sizeof( FIOMSG_RX_FRAME ) - 1
		  + FIOMAN_FRAME_NO_152_SIZE, GFP_KERNEL ) ) )
	{
		/* kmalloc succeeded, therefore init buffer */
		fioman_rx_frame_init(p_sys_fiod, p_rx);
		FIOMSG_PAYLOAD( p_rx )->frame_no = frame_no;
		p_rx->len = FIOMAN_FRAME_NO_152_SIZE;
	}
	else
	{
		/* Could not kalloc */
		return ( ERR_PTR( -ENOMEM ) );
	}

	/* Return what we prepared */
	return ( (void *)p_rx );
}

/*****************************************************************************/
/*
This function is used to ready response frame 177 to be placed into the
response frame list for this fiod for this port.
*/
/*****************************************************************************/

void *
fioman_ready_frame_177
(
	FIOMAN_SYS_FIOD	*p_sys_fiod		/* FIOD of destined frame */
)
{
	FIOMSG_RX_FRAME	*p_rx;		/* Ptr of frame buffer */

	/* kalloc the actual frame 177 for this port */
	/* -1 is for the one byte of frame payload defined in FIOMSG_RX_FRAME */
	if ( ( p_rx = (FIOMSG_RX_FRAME *)kzalloc( sizeof( FIOMSG_RX_FRAME ) - 1
		+ FIOMAN_FRAME_NO_177_SIZE, GFP_KERNEL ) ) )
	{
		/* kmalloc succeeded, therefore init buffer */
		fioman_rx_frame_init(p_sys_fiod, p_rx);
		FIOMSG_PAYLOAD( p_rx )->frame_no = FIOMAN_FRAME_NO_177;
		p_rx->rx_func = &fioman_rx_frame_177;
		p_rx->len = FIOMAN_FRAME_NO_177_SIZE;
	}
	else
	{
		/* Could not kalloc */
		return ( ERR_PTR( -ENOMEM ) );
	}

	/* Return what we prepared */
	return ( (void *)p_rx );
}

/*****************************************************************************/
/*****************************************************************************/
/*
This function is used to ready response frame 179 to be placed into the
response frame list for this fiod for this port.
*/
/*****************************************************************************/

void *
fioman_ready_frame_179
(
	FIOMAN_SYS_FIOD	*p_sys_fiod		/* FIOD of destined frame */
)
{
	FIOMSG_RX_FRAME	*p_rx;		/* Ptr of frame buffer */

	/* kalloc the actual frame 179 for this port */
	/* -1 is for the one byte of frame payload defined in FIOMSG_RX_FRAME */
	if ( ( p_rx = (FIOMSG_RX_FRAME *)kzalloc( sizeof( FIOMSG_RX_FRAME ) - 1
		+ FIOMAN_FRAME_NO_179_SIZE, GFP_KERNEL ) ) )
	{
		/* kmalloc succeeded, therefore init buffer */
		fioman_rx_frame_init(p_sys_fiod, p_rx);
		FIOMSG_PAYLOAD( p_rx )->frame_no = FIOMAN_FRAME_NO_179;
		p_rx->rx_func = &fioman_rx_frame_179;
		p_rx->len = FIOMAN_FRAME_NO_179_SIZE;
	}
	else
	{
		/* Could not kalloc */
		return ( ERR_PTR( -ENOMEM ) );
	}

	/* Return what we prepared */
	return ( (void *)p_rx );
}

/*****************************************************************************/
/*****************************************************************************/
/*
This function is used to ready response frame 180 to be placed into the
response frame list for this fiod for this port.
*/
/*****************************************************************************/

void *
fioman_ready_frame_180
(
	FIOMAN_SYS_FIOD	*p_sys_fiod		/* FIOD of destined frame */
)
{
	FIOMSG_RX_FRAME	*p_rx;		/* Ptr of frame buffer */
	u16	rx_len = FIOMAN_FRAME_NO_180_SIZE;

	if ((p_sys_fiod->fiod.fiod == FIOTS1) || (p_sys_fiod->fiod.fiod == FIOTS2))
		/* 2070-8 or 2070-2N */
		rx_len = FIOMAN_FRAME_NO_180_SIZE + 7;	/* 2070-8 or 2070-2N */

	/* kalloc the actual frame 180 for this port */
	/* -1 is for the one byte of frame payload defined in FIOMSG_RX_FRAME */
	if ( ( p_rx = (FIOMSG_RX_FRAME *)kzalloc( sizeof( FIOMSG_RX_FRAME ) - 1
		+ rx_len, GFP_KERNEL ) ) )
	{
		/* kmalloc succeeded, therefore init buffer */
		fioman_rx_frame_init(p_sys_fiod, p_rx);
		FIOMSG_PAYLOAD( p_rx )->frame_no = FIOMAN_FRAME_NO_180;
		p_rx->rx_func = &fioman_rx_frame_180;
		p_rx->len = rx_len;
	}
	else
	{
		/* Could not kalloc */
		return ( ERR_PTR( -ENOMEM ) );
	}

	/* Return what we prepared */
	return ( (void *)p_rx );
}

/*****************************************************************************/

/*****************************************************************************/
/*
This function is used to ready response frame 181 to be placed into the
response frame list for this fiod for this port.
*/
/*****************************************************************************/

void *
fioman_ready_frame_181
(
	FIOMAN_SYS_FIOD	*p_sys_fiod		/* FIOD of destined frame */
)
{
	FIOMSG_RX_FRAME	*p_rx;		/* Ptr of frame buffer */
	u16	rx_len = FIOMAN_FRAME_NO_181_SIZE;

	if ((p_sys_fiod->fiod.fiod == FIOTS1) || (p_sys_fiod->fiod.fiod == FIOTS2))
		/* 2070-8 or 2070-2N */
		rx_len = FIOMAN_FRAME_NO_181_SIZE + 7;	/* 2070-8 or 2070-2N */

	/* kalloc the actual frame 181 for this port */
	/* -1 is for the one byte of frame payload defined in FIOMSG_RX_FRAME */
	if ( ( p_rx = (FIOMSG_RX_FRAME *)kzalloc( sizeof( FIOMSG_RX_FRAME ) - 1 + rx_len, GFP_KERNEL ) ) )
	{
		/* kmalloc succeeded, therefore init buffer */
		fioman_rx_frame_init(p_sys_fiod, p_rx);
		FIOMSG_PAYLOAD( p_rx )->frame_no = FIOMAN_FRAME_NO_181;
		p_rx->rx_func = &fioman_rx_frame_181;
		p_rx->len = rx_len;
	}
	else
	{
		/* Could not kalloc */
		return ( ERR_PTR( -ENOMEM ) );
	}

	/* Return what we prepared */
	return ( (void *)p_rx );
}

/*****************************************************************************/

/*****************************************************************************/
/*
This function is used to ready response frame 182 to be placed into the
response frame list for this fiod for this port.
*/
/*****************************************************************************/

void *
fioman_ready_frame_182
(
	FIOMAN_SYS_FIOD	*p_sys_fiod		/* FIOD of destined frame */
)
{
	FIOMSG_RX_FRAME	*p_rx;		/* Ptr of frame buffer */

	/* kalloc the actual frame 182 for this port */
	/* -1 is for the one byte of frame payload defined in FIOMSG_RX_FRAME */
	if ( ( p_rx = (FIOMSG_RX_FRAME *)kzalloc( sizeof( FIOMSG_RX_FRAME ) - 1 + FIOMAN_FRAME_NO_182_SIZE, GFP_KERNEL ) ) )
	{
		/* kmalloc succeeded, therefore init buffer */
		fioman_rx_frame_init(p_sys_fiod, p_rx);
		FIOMSG_PAYLOAD( p_rx )->frame_no = FIOMAN_FRAME_NO_182;
		p_rx->rx_func = &fioman_rx_frame_182;
		p_rx->len = FIOMAN_FRAME_NO_182_SIZE;
	}
	else
	{
		/* Could not kalloc */
		return ( ERR_PTR( -ENOMEM ) );
	}

	/* Return what we prepared */
	return ( (void *)p_rx );
}

/*****************************************************************************/

/*****************************************************************************/
/*
This function is used to ready response frame 183 to be placed into the
response frame list for this fiod for this port.
*/
/*****************************************************************************/

void *
fioman_ready_frame_183
(
	FIOMAN_SYS_FIOD	*p_sys_fiod		/* FIOD of destined frame */
)
{
	FIOMSG_RX_FRAME	*p_rx;		/* Ptr of frame buffer */

	/* kalloc the actual frame 183 for this port */
	/* -1 is for the one byte of frame payload defined in FIOMSG_RX_FRAME */
	if ( ( p_rx = (FIOMSG_RX_FRAME *)kzalloc( sizeof( FIOMSG_RX_FRAME ) - 1
		+ FIOMAN_FRAME_NO_183_SIZE, GFP_KERNEL ) ) )
	{
		/* kmalloc succeeded, therefore init buffer */
		fioman_rx_frame_init(p_sys_fiod, p_rx);
		FIOMSG_PAYLOAD( p_rx )->frame_no = FIOMAN_FRAME_NO_183;
		p_rx->rx_func = &fioman_rx_frame_183;
		p_rx->len = FIOMAN_FRAME_NO_183_SIZE;
	}
	else
	{
		/* Could not kalloc */
		return ( ERR_PTR( -ENOMEM ) );
	}

	/* Return what we prepared */
	return ( (void *)p_rx );
}

/*****************************************************************************/
/*****************************************************************************/
/*
This function is used to ready response frame 188 to be placed into the
response frame list for this fiod for this port.
*/
/*****************************************************************************/

void *
fioman_ready_frame_188
(
	FIOMAN_SYS_FIOD	*p_sys_fiod		/* FIOD of destined frame */
)
{
	FIOMSG_RX_FRAME	*p_rx;		/* Ptr of frame buffer */

	/* kalloc the actual frame 188 for this port */
	/* -1 is for the one byte of frame payload defined in FIOMSG_RX_FRAME */
	if ( ( p_rx = (FIOMSG_RX_FRAME *)kzalloc( sizeof( FIOMSG_RX_FRAME ) - 1
		+ FIOMAN_FRAME_NO_188_SIZE, GFP_KERNEL ) ) )
	{
		/* kmalloc succeeded, therefore init buffer */
		fioman_rx_frame_init(p_sys_fiod, p_rx);
		FIOMSG_PAYLOAD( p_rx )->frame_no = FIOMAN_FRAME_NO_188;
		p_rx->rx_func = &fioman_rx_frame_188;
		p_rx->len = FIOMAN_FRAME_NO_188_SIZE;
	}
	else
	{
		/* Could not kalloc */
		return ( ERR_PTR( -ENOMEM ) );
	}

	/* Return what we prepared */
	return ( (void *)p_rx );
}

/*****************************************************************************/
/*****************************************************************************/
/*
This function is used to ready response frame 189 to be placed into the
response frame list for this fiod for this port.
*/
/*****************************************************************************/

void *
fioman_ready_frame_189
(
	FIOMAN_SYS_FIOD	*p_sys_fiod		/* FIOD of destined frame */
)
{
	FIOMSG_RX_FRAME	*p_rx;		/* Ptr of frame buffer */

	/* kalloc the actual frame 189 for this port */
	/* -1 is for the one byte of frame payload defined in FIOMSG_RX_FRAME */
	if ( ( p_rx = (FIOMSG_RX_FRAME *)kzalloc( sizeof( FIOMSG_RX_FRAME ) - 1
		+ FIOMAN_FRAME_NO_189_SIZE, GFP_KERNEL ) ) )
	{
		/* kmalloc succeeded, therefore init buffer */
		fioman_rx_frame_init(p_sys_fiod, p_rx);
		FIOMSG_PAYLOAD( p_rx )->frame_no = FIOMAN_FRAME_NO_189;
		p_rx->rx_func = &fioman_rx_frame_189;
		p_rx->len = FIOMAN_FRAME_NO_189_SIZE;
	}
	else
	{
		/* Could not kalloc */
		return ( ERR_PTR( -ENOMEM ) );
	}

	/* Return what we prepared */
	return ( (void *)p_rx );
}

/*****************************************************************************/
/*
This function is used to ready response frame 190 to be placed into the
response frame list for this fiod for this port.
*/
/*****************************************************************************/

void *
fioman_ready_frame_190
(
	FIOMAN_SYS_FIOD	*p_sys_fiod		/* FIOD of destined frame */
)
{
	FIOMSG_RX_FRAME	*p_rx;		/* Ptr of frame buffer */

	/* kalloc the actual frame 190 for this port */
	/* -1 is for the one byte of frame payload defined in FIOMSG_RX_FRAME */
	if ( ( p_rx = (FIOMSG_RX_FRAME *)kzalloc( sizeof( FIOMSG_RX_FRAME ) - 1
		+ FIOMAN_FRAME_NO_190_SIZE, GFP_KERNEL ) ) )
	{
		/* kmalloc succeeded, therefore init buffer */
		fioman_rx_frame_init(p_sys_fiod, p_rx);
		FIOMSG_PAYLOAD( p_rx )->frame_no = FIOMAN_FRAME_NO_190;
		p_rx->len = FIOMAN_FRAME_NO_190_SIZE;
	}
	else
	{
		/* Could not kalloc */
		return ( ERR_PTR( -ENOMEM ) );
	}

	/* Return what we prepared */
	return ( (void *)p_rx );
}

/*****************************************************************************/
/*****************************************************************************/
/*
This function is used to ready response frame 195 to be placed into the
response frame list for this fiod for this port.
*/
/*****************************************************************************/

void *
fioman_ready_frame_195
(
	FIOMAN_SYS_FIOD	*p_sys_fiod		/* FIOD of destined frame */
)
{
	FIOMSG_RX_FRAME	*p_rx;		/* Ptr of frame buffer */

	/* kalloc the actual frame 195 for this port */
	/* -1 is for the one byte of frame payload defined in FIOMSG_RX_FRAME */
	if ( ( p_rx = (FIOMSG_RX_FRAME *)kzalloc( sizeof( FIOMSG_RX_FRAME ) - 1
		+ FIOMAN_FRAME_NO_195_SIZE, GFP_KERNEL ) ) )
	{
		/* kmalloc succeeded, therefore init buffer */
		fioman_rx_frame_init(p_sys_fiod, p_rx);
		FIOMSG_PAYLOAD( p_rx )->frame_no = FIOMAN_FRAME_NO_195;
		p_rx->rx_func = &fioman_rx_frame_195;
		p_rx->len = FIOMAN_FRAME_NO_195_SIZE;
	}
	else
	{
		/* Could not kalloc */
		return ( ERR_PTR( -ENOMEM ) );
	}

	/* Return what we prepared */
	return ( (void *)p_rx );
}

/*****************************************************************************/

/*****************************************************************************/
/*****************************************************************************/
/*
This function is used to ready response frame 128 to be placed into the
response frame list for this fiod for this port.
*/
/*****************************************************************************/

void *
fioman_ready_frame_128
(
	FIOMAN_SYS_FIOD	*p_sys_fiod		/* FIOD of destined frame */
)
{
	FIOMSG_RX_FRAME	*p_rx;		/* Ptr of frame buffer */

	/* kalloc the actual frame 128 for this port */
	/* -1 is for the one byte of frame payload defined in FIOMSG_RX_FRAME */
	if ( ( p_rx = (FIOMSG_RX_FRAME *)kzalloc( sizeof( FIOMSG_RX_FRAME ) - 1
		+ FIOMAN_FRAME_NO_128_SIZE, GFP_KERNEL ) ) )
	{
		/* kmalloc succeeded, therefore init buffer */
		fioman_rx_frame_init(p_sys_fiod, p_rx);
		FIOMSG_PAYLOAD( p_rx )->frame_no = FIOMAN_FRAME_NO_128;
		p_rx->len = FIOMAN_FRAME_NO_128_SIZE;
	}
	else
	{
		/* Could not kalloc */
		return ( ERR_PTR( -ENOMEM ) );
	}

	/* Return what we prepared */
	return ( (void *)p_rx );
}

/*****************************************************************************/
/*****************************************************************************/
/*
This function is used to ready response frame 129 to be placed into the
response frame list for this fiod for this port.
*/
/*****************************************************************************/

void *
fioman_ready_frame_129
(
	FIOMAN_SYS_FIOD	*p_sys_fiod		/* FIOD of destined frame */
)
{
	FIOMSG_RX_FRAME	*p_rx;		/* Ptr of frame buffer */

	/* kalloc the actual frame 129 for this port */
	/* -1 is for the one byte of frame payload defined in FIOMSG_RX_FRAME */
	if ( ( p_rx = (FIOMSG_RX_FRAME *)kzalloc( sizeof( FIOMSG_RX_FRAME ) - 1
		+ FIOMAN_FRAME_NO_129_SIZE, GFP_KERNEL ) ) )
	{
		/* kmalloc succeeded, therefore init buffer */
		fioman_rx_frame_init(p_sys_fiod, p_rx);
		FIOMSG_PAYLOAD( p_rx )->frame_no = FIOMAN_FRAME_NO_129;
		p_rx->rx_func = &fioman_rx_frame_129;
		p_rx->len = FIOMAN_FRAME_NO_129_SIZE;
	}
	else
	{
		/* Could not kalloc */
		return ( ERR_PTR( -ENOMEM ) );
	}

	/* Return what we prepared */
	return ( (void *)p_rx );
}

/*****************************************************************************/

/*****************************************************************************/
/*
This function is used to ready response frame 131 to be placed into the
response frame list for this fiod for this port.
*/
/*****************************************************************************/

void *
fioman_ready_frame_131
(
	FIOMAN_SYS_FIOD	*p_sys_fiod		/* FIOD of destined frame */
)
{
	FIOMSG_RX_FRAME	*p_rx;		/* Ptr of frame buffer */

	/* kalloc the actual frame 131 for this port */
	/* -1 is for the one byte of frame payload defined in FIOMSG_RX_FRAME */
	if ( ( p_rx = (FIOMSG_RX_FRAME *)kzalloc( sizeof( FIOMSG_RX_FRAME ) - 1
		+ FIOMAN_FRAME_NO_131_SIZE, GFP_KERNEL ) ) )
	{
		/* kmalloc succeeded, therefore init buffer */
		fioman_rx_frame_init(p_sys_fiod, p_rx);
		FIOMSG_PAYLOAD( p_rx )->frame_no = FIOMAN_FRAME_NO_131;
		p_rx->rx_func = &fioman_rx_frame_131;
		p_rx->len = FIOMAN_FRAME_NO_131_SIZE;
		p_rx->info.frequency = FIO_HZ_1;
	}
	else
	{
		/* Could not kalloc */
		return ( ERR_PTR( -ENOMEM ) );
	}

	/* Return what we prepared */
	return ( (void *)p_rx );
}

/*****************************************************************************/
/*****************************************************************************/
/*
This function is called when a frame 129 is RX'ed.
*/
/*****************************************************************************/

void
fioman_rx_frame_129
(
	FIOMSG_RX_FRAME		*p_rx_frame		/* Frame received */
)
{
	/*FIOMAN_SYS_FIOD		*p_sys_fiod;*/	/* For access to System info */

	/* Get access to system info */
	/*p_sys_fiod = (FIOMAN_SYS_FIOD *)p_rx_frame->fioman_context;*/

/* TEG DEL */
/*pr_debug( "UPDATING Frame 129\n" );*/
/* TEG DEL */
}

/*****************************************************************************/
/*****************************************************************************/
/*
This function is called when a frame 131 is RX'ed.
*/
/*****************************************************************************/

void
fioman_rx_frame_131
(
	FIOMSG_RX_FRAME		*p_rx_frame		/* Frame received */
)
{
	/*FIOMAN_SYS_FIOD		*p_sys_fiod;*/	/* For access to System info */

	/* Get access to system info */
	/*p_sys_fiod = (FIOMAN_SYS_FIOD *)p_rx_frame->fioman_context;*/

/* TEG DEL */
/*pr_debug( "UPDATING Frame 131\n" );*/
/* TEG DEL */
}

/*****************************************************************************/
/*****************************************************************************/
/*
This function is called when a frame 138/139/140/141 is RX'ed.
*/
/*****************************************************************************/

void
fioman_rx_frame_138_141
(
	FIOMSG_RX_FRAME		*p_rx_frame		/* Frame received */
)
{
	FIOMAN_SYS_FIOD		*p_sys_fiod;	/* For access to System info */
	int i;
	unsigned long flags;

	/* Get access to system info */
	p_sys_fiod = (FIOMAN_SYS_FIOD *)p_rx_frame->fioman_context;

/* TEG DEL */
/*pr_debug( "UPDATING Frame 138/139/140/141\n" );*/
/* TEG DEL */

	/* Save data into FIOD System buffers */
	spin_lock_irqsave(&p_sys_fiod->lock, flags);
	for(i=0;i<p_rx_frame->len-3;i++) {
		p_sys_fiod->inputs_raw[i] = FIOMSG_PAYLOAD(p_rx_frame)->frame_info[i];
	}
	spin_unlock_irqrestore(&p_sys_fiod->lock, flags);

		/*pr_debug("UPDATING Frame 138/141: %x %x %x %x %x\n",
				FIOMSG_PAYLOAD(p_rx_frame)->frame_info[0],
				FIOMSG_PAYLOAD(p_rx_frame)->frame_info[1],
				FIOMSG_PAYLOAD(p_rx_frame)->frame_info[2],
				FIOMSG_PAYLOAD(p_rx_frame)->frame_info[3],
				FIOMSG_PAYLOAD(p_rx_frame)->frame_info[4]);*/
/* TEG DEL */
/* Set BIT 100 for testing */
/*(void)memset( p_sys_fiod->inputs_raw, 0, sizeof( p_sys_fiod->inputs_raw ) );
FIO_BIT_SET( p_sys_fiod->inputs_raw, 100 );*/
/* TEG DEL */
}

/*****************************************************************************/
/*****************************************************************************/
/*
This function is called when a frame 148/151 is RX'ed.
*/
/*****************************************************************************/

void
fioman_rx_frame_148_151
(
	FIOMSG_RX_FRAME		*p_rx_frame		/* Frame received */
)
{
	FIOMAN_SYS_FIOD		*p_sys_fiod;	/* For access to System info */
	int i;
	unsigned long flags;

	/* Get access to system info */
	p_sys_fiod = (FIOMAN_SYS_FIOD *)p_rx_frame->fioman_context;


	/* Save data into FIOD System buffers */
	spin_lock_irqsave(&p_sys_fiod->lock, flags);
	for(i=0;i<2;i++) {
		p_sys_fiod->inputs_raw[i] = FIOMSG_PAYLOAD(p_rx_frame)->frame_info[i+32];
	}
	spin_unlock_irqrestore(&p_sys_fiod->lock, flags);
	/* TEG DEL */
		/*pr_debug("UPDATING Frame 148/151: %x %x\n",
			p_sys_fiod->inputs_raw[0], p_sys_fiod->inputs_raw[1] );*/
	/* TEG DEL */

}

/*****************************************************************************/
/*****************************************************************************/
/*
This function is called when a frame 177 is RX'ed.
*/
/*****************************************************************************/

void
fioman_rx_frame_177
(
	FIOMSG_RX_FRAME		*p_rx_frame		/* Frame received */
)
{
	FIOMAN_SYS_FIOD		*p_sys_fiod;	/* For access to System info */
	unsigned long flags;

	/* Get access to system info */
	p_sys_fiod = (FIOMAN_SYS_FIOD *)p_rx_frame->fioman_context;

/* TEG DEL */
/*pr_debug( "UPDATING Frame 177\n" );*/
/* TEG DEL */

	/* Save data into FIOD System buffers */
	spin_lock_irqsave(&p_sys_fiod->lock, flags);
	p_sys_fiod->status = FIOMSG_PAYLOAD(p_rx_frame)->frame_info[0]; /* status bits */
	/* rx err count */
	/* tx err count */
	/* timestamp */

	/* reset status error flags */
pr_debug("fioman_rx_frame_177: status=%x\n", p_sys_fiod->status);
	if (p_sys_fiod->status & 0xc1) {/* P, E or W bits */
		p_sys_fiod->status_reset = p_sys_fiod->status & 0xc1;
		/* schedule frame 51 to reconfigure input point filters */
                /* and reconfigure input transition monitoring */
		fiomsg_tx_add_frame(FIOMSG_P_PORT(p_sys_fiod->fiod.port), fioman_ready_frame_51(p_sys_fiod));
                fiomsg_rx_add_frame(FIOMSG_P_PORT(p_sys_fiod->fiod.port), fioman_ready_frame_179(p_sys_fiod));
	} else
		p_sys_fiod->status_reset = 0;

	spin_unlock_irqrestore(&p_sys_fiod->lock, flags);
}

/*****************************************************************************/
/*****************************************************************************/
/*
This function is called when a frame 179 is RX'ed.
*/
/*****************************************************************************/

void
fioman_rx_frame_179
(
	FIOMSG_RX_FRAME		*p_rx_frame		/* Frame received */
)
{
	/*FIOMAN_SYS_FIOD		*p_sys_fiod;*/	/* For access to System info */
	unsigned char		status;

	/* Get access to system info */
	/*p_sys_fiod = (FIOMAN_SYS_FIOD *)p_rx_frame->fioman_context;*/

/* TEG DEL */
/*printk( KERN_ALERT "UPDATING Frame 179\n" );*/
/* TEG DEL */
	status = FIOMSG_PAYLOAD(p_rx_frame)->frame_info[0]; /* status bits */
	/* If status indicates error, maybe reset all filters to default? */
	pr_debug("fioman_rx_frame_179: status=%x\n", status);

}

/*****************************************************************************/
/*****************************************************************************/
/*
This function is called when a frame 180 is RX'ed.
*/
/*****************************************************************************/

void
fioman_rx_frame_180
(
	FIOMSG_RX_FRAME		*p_rx_frame		/* Frame received */
)
{
	FIOMAN_SYS_FIOD		*p_sys_fiod;	/* For access to System info */
	int i;
	unsigned long flags;
	
	/* Get access to system info */
	p_sys_fiod = (FIOMAN_SYS_FIOD *)p_rx_frame->fioman_context;

/* TEG DEL */
/*pr_debug( "UPDATING Frame 180\n" );*/
/* TEG DEL */
	pr_debug("fioman_rx_frame_180: reading %d input bytes\n",p_rx_frame->len-7);

	/* Save data into FIOD System buffers */
	spin_lock_irqsave(&p_sys_fiod->lock, flags);
	for(i=0;i<p_rx_frame->len-7;i++) {
		p_sys_fiod->inputs_raw[i] = FIOMSG_PAYLOAD(p_rx_frame)->frame_info[i];
	}
	spin_unlock_irqrestore(&p_sys_fiod->lock, flags);

/* TEG DEL */
/* Set BIT 100 for testing */
/*(void)memset( p_sys_fiod->inputs_raw, 0, sizeof( p_sys_fiod->inputs_raw ) );
FIO_BIT_SET( p_sys_fiod->inputs_raw, 100 );*/
/* TEG DEL */
}

/*****************************************************************************/

/*****************************************************************************/
/*
This function is called when a frame 181 is RX'ed.
*/
/*****************************************************************************/

void
fioman_rx_frame_181
(
	FIOMSG_RX_FRAME		*p_rx_frame		/* Frame received */
)
{
	FIOMAN_SYS_FIOD		*p_sys_fiod;	/* For access to System info */
	int i;
	unsigned long flags;

	/* Get access to system info */
	p_sys_fiod = (FIOMAN_SYS_FIOD *)p_rx_frame->fioman_context;

/* TEG DEL */
/*pr_debug( "UPDATING Frame 181\n" );*/
/* TEG DEL */

	/* Save data into FIOD System buffers */
	spin_lock_irqsave(&p_sys_fiod->lock, flags);
	for(i=0;i<p_rx_frame->len-7;i++) {
		p_sys_fiod->inputs_filtered[i] = FIOMSG_PAYLOAD(p_rx_frame)->frame_info[i];
	}
	spin_unlock_irqrestore(&p_sys_fiod->lock, flags);

/* TEG DEL */
/* Set BIT 100 for testing */
/*(void)memset( p_sys_fiod->inputs_raw, 0, sizeof( p_sys_fiod->inputs_raw ) );
FIO_BIT_SET( p_sys_fiod->inputs_raw, 100 );*/
/* TEG DEL */
}

/*****************************************************************************/

/*****************************************************************************/
/*
This function is called when a frame 182 is RX'ed.
*/
/*****************************************************************************/

void
fioman_rx_frame_182
(
	FIOMSG_RX_FRAME		*p_rx_frame		/* Frame received */
)
{
	FIOMAN_SYS_FIOD		*p_sys_fiod;	/* For access to System info */
	FIOMAN_APP_FIOD		*p_app_fiod;	/* Ptr to app fiod structure */
	int i;
	unsigned long flags;
        unsigned char block, count;
	FIO_TRANS_BUFFER entry;
	struct list_head *p_app_elem;           /* Ptr to app element being examined */

	/* Get access to system info */
	p_sys_fiod = (FIOMAN_SYS_FIOD *)p_rx_frame->fioman_context;

        /* Update transition buffers */
        spin_lock_irqsave(&p_sys_fiod->lock, flags);
        /* Check block number */
        block = FIOMSG_PAYLOAD(p_rx_frame)->frame_info[0];
        /* Get number of entries in this frame */
        count = FIOMSG_PAYLOAD(p_rx_frame)->frame_info[1];
        /* Check status byte */
        if( FIOMSG_PAYLOAD(p_rx_frame)->frame_info[2+(count*3)] & 0xc ) {
                // Overflow condition
                list_for_each( p_app_elem, &p_sys_fiod->app_fiod_list ) {
                        /* Get a ptr to this list entry */
                        p_app_fiod = list_entry( p_app_elem, FIOMAN_APP_FIOD, sys_elem );
                        if (p_app_fiod->input_transition_map != 0)
                                p_app_fiod->transition_status = FIO_TRANS_FIOD_OVERRUN;
                }
        }
        /* Update each app transition fifo */
        for(i=0;i<count;i++) {
		entry.input_point = FIOMSG_PAYLOAD(p_rx_frame)->frame_info[2+(i*3)]&0x7f;
		entry.state = (FIOMSG_PAYLOAD(p_rx_frame)->frame_info[2+(i*3)]&0x80)?1:0;
		entry.timestamp = FIOMSG_PAYLOAD(p_rx_frame)->frame_info[2+(i*3)+2];
		entry.timestamp |= FIOMSG_PAYLOAD(p_rx_frame)->frame_info[2+(i*3)+1]<<8;
		// push entry to each app_fiod fifo
                list_for_each( p_app_elem, &p_sys_fiod->app_fiod_list ) {
                        /* Get a ptr to this list entry */
                        p_app_fiod = list_entry( p_app_elem, FIOMAN_APP_FIOD, sys_elem );
                        if ((p_app_fiod->transition_status != FIO_TRANS_APP_OVERRUN)
                                && ((entry.input_point == 0x7f) /* rollover entry */
                                || FIO_BIT_TEST(p_app_fiod->input_transition_map, entry.input_point))) {
                                if (FIOMAN_FIFO_AVAIL(p_app_fiod->transition_fifo) < sizeof(FIO_TRANS_BUFFER)) {
                                        // app fifo status is overflow
                                        if (p_app_fiod->transition_status == FIO_TRANS_SUCCESS)
                                                p_app_fiod->transition_status = FIO_TRANS_APP_OVERRUN;
                                } else {
                                        FIOMAN_FIFO_PUT(p_app_fiod->transition_fifo, &entry, sizeof(FIO_TRANS_BUFFER));
                                        pr_debug("fioman_rx_frame_182:transition:ip=%d state=%d time=%d:fifo@%p=%d, st=%d\n",
                                                entry.input_point, entry.state, entry.timestamp, &p_app_fiod->transition_fifo,
                                                kfifo_len(&p_app_fiod->transition_fifo), p_app_fiod->transition_status);
                                }
                        }
                }
        }
	spin_unlock_irqrestore(&p_sys_fiod->lock, flags);

/* TEG DEL */
pr_debug( "UPDATING Frame 182: block %d, %d entries\n", block, count );
/* TEG DEL */

}

/*****************************************************************************/

/*****************************************************************************/
/*
This function is called when a frame 183 is RX'ed.
*/
/*****************************************************************************/

void
fioman_rx_frame_183
(
	FIOMSG_RX_FRAME		*p_rx_frame		/* Frame received */
)
{
	/*FIOMAN_SYS_FIOD		*p_sys_fiod;*/	/* For access to System info */

	/* Get access to system info */
	/*p_sys_fiod = (FIOMAN_SYS_FIOD *)p_rx_frame->fioman_context;*/
	
/* TEG DEL */
/*pr_debug( "UPDATING Frame 183\n" );*/
/* handle status bits from response frame */
/* TEG DEL */
}

/*****************************************************************************/
/*****************************************************************************/
/*
This function is called when a frame 188 is RX'ed.
*/
/*****************************************************************************/

void
fioman_rx_frame_188
(
	FIOMSG_RX_FRAME		*p_rx_frame		/* Frame received */
)
{
	/*FIOMAN_SYS_FIOD		*p_sys_fiod;*/	/* For access to System info */

	/* Get access to system info */
	/*p_sys_fiod = (FIOMAN_SYS_FIOD *)p_rx_frame->fioman_context;*/
	
/* TEG DEL */
/*pr_debug( "UPDATING Frame 188\n" );*/
/* handle status bits from response frame */
/* TEG DEL */
}

/*****************************************************************************/
/*****************************************************************************/
/*
This function is called when a frame 189 is RX'ed.
*/
/*****************************************************************************/

void
fioman_rx_frame_189
(
	FIOMSG_RX_FRAME		*p_rx_frame		/* Frame received */
)
{
	FIOMAN_SYS_FIOD		*p_sys_fiod;	/* For access to System info */
	unsigned long flags;

	/* Get access to system info */
	p_sys_fiod = (FIOMAN_SYS_FIOD *)p_rx_frame->fioman_context;

	/* Save data into FIOD System buffers */
	spin_lock_irqsave(&p_sys_fiod->lock, flags);
	if (FIOMSG_PAYLOAD(p_rx_frame)->frame_info[30] & 0x1) {
		p_sys_fiod->cmu_config_change_count++;
	}
	spin_unlock_irqrestore(&p_sys_fiod->lock, flags);
}

/*****************************************************************************/
/*
This function is called when a frame 195 is RX'ed.
*/
/*****************************************************************************/

void
fioman_rx_frame_195
(
	FIOMSG_RX_FRAME		*p_rx_frame		/* Frame received */
)
{
	FIOMAN_SYS_FIOD		*p_sys_fiod;	/* For access to System info */
	unsigned long flags;

	/* Get access to system info */
	p_sys_fiod = (FIOMAN_SYS_FIOD *)p_rx_frame->fioman_context;

	/* Save data into FIOD System buffers */
	spin_lock_irqsave(&p_sys_fiod->lock, flags);
	if (FIOMSG_PAYLOAD(p_rx_frame)->frame_info[18] & 0x1) {
		p_sys_fiod->cmu_config_change_count++;
	}
	spin_unlock_irqrestore(&p_sys_fiod->lock, flags);
}
/*****************************************************************************/

