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

#ifndef __TOD_H__
#define __TOD_H__
#include <sys/time.h>

struct dst_info
{
	char type;
	union dst_types_union
	{
		struct dst_absolute_struct
		{
			int secs_from_epoch_to_transition;
			int seconds_to_adjust;
		} absolute;

		struct dst_generic_struct
		{
			char month;
			char dom_type;	// 0=dom, 1=forward_occurrences, 2=reverse_occurrences
			union dst_gen_dom_union
			{
				char dom;
				// e.g. second Saturday of month
				// e.g. first Sunday on or after oct 9
				struct dst_gen1_struct
				{
					char dow;			// day of week (sun-sat)
					char occur;			// number of occurrences
					char on_after_dom;		// day of month
				} forward_occurrences_of_dow;
				// e.g. second to last Thursday of month
				// e.g. first Sunday on or before oct 9
				struct dst_gen2_struct
				{
					char dow;			// day of week (sun-sat)
					char occur;			// number of occurrences
					char on_before_dom;		// day of month
				} reverse_occurrences_of_dow;
			} generic_dom;
			int secs_from_midnight_to_transition;
			int seconds_to_adjust;
		} generic;
	} begin, end;
};

typedef	struct dst_info	dst_info_t;

enum
{
	TOD_TIMESRC_LINESYNC,
	TOD_TIMESRC_RTCSQWR,
	TOD_TIMESRC_CRYSTAL,
	TOD_TIMESRC_EXTERNAL1,
	TOD_TIMESRC_EXTERNAL2
} TOD_TIMESRC_ENUM;

char *tod_apiver(void);

int tod_get(struct timeval *tv, int *tzsec_off, int *dst_off);

int tod_set(const struct timeval *tv, const int *tzsec_off);

int tod_get_dst_info(dst_info_t *dst_info);

int tod_set_dst_info(const dst_info_t *dst_info);

int tod_get_dst_state(void);

int tod_set_dst_state(int state);

int tod_get_timesrc(void);

int tod_get_timesrc_freq(void);

int tod_set_timesrc(int timesrc);

int tod_request_onchange_signal(int);

int tod_cancel_onchange_signal(void);

int tod_request_tick_signal(int);

int tod_cancel_tick_signal(int);

#endif /* __TOD_H__ */
