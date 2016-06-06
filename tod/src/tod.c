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

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <time.h>
#include <limits.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>
#include <atc.h>
#include "tod.h"

#ifndef __isdigit_char
#define __isdigit_char(c)   (((unsigned char)((c) - '0')) <= 9)  
#endif
#ifndef __isleap
#define __isleap(y) ( !((y) % 4) && ( ((y) % 100) || !((y) %400) ) )
#endif


char *tod_apiver(void)
{
	return "APIRI, 1.1, 2.17";
}

int tod_ioctl_call(int command, int arg)
{
	int ret = 0;
	int tod_fd;
	
	/* open tod device */
	if ((ret = open("/dev/tod", O_RDONLY)) < 0)
		return ret;
	tod_fd = ret;
	ret = ioctl(tod_fd, command, &arg);
	close (tod_fd);
	
	return ret;
	
}

int tod_get_timesrc(void)
{
	int arg = 0;
	
	return tod_ioctl_call(ATC_TOD_GET_TIMESRC, arg);
}

int tod_get_timesrc_freq(void)
{
	int arg = 0;
	
	return tod_ioctl_call(ATC_TOD_GET_INPUT_FREQ, arg);
}

int tod_set_timesrc(int timesrc)
{
	return tod_ioctl_call(ATC_TOD_SET_TIMESRC, timesrc);
}

int tod_request_tick_signal(int signal)
{
	int tod_fd;
	int ret, arg;
	
	if ((ret = open("/dev/tod", O_RDONLY)) < 0)
		return ret;
	tod_fd = ret;
	arg = signal;
	ret = ioctl(tod_fd, ATC_TOD_REQUEST_TICK_SIG, arg);
	if (ret < 0) {
		close(tod_fd);
		return ret;
	}
	
	return tod_fd;
}

int tod_cancel_tick_signal(int fd)
{
	int ret, arg = 0;
	
	ret = ioctl(fd, ATC_TOD_CANCEL_TICK_SIG, &arg);
	if (ret < 0)
		return ret;
		
	close(fd);
	return 0;
}

int tod_request_onchange_signal(int signal)
{
	int tod_fd;
	int ret, arg;
	
	if ((ret = open("/dev/tod", O_RDONLY)) < 0)
		return ret;
	tod_fd = ret;
	arg = signal;
	ret = ioctl(tod_fd, ATC_TOD_REQUEST_ONCHANGE_SIG, arg);
	if (ret < 0) {
		close(tod_fd);
		return ret;
	}
		
	return tod_fd;
}

int tod_cancel_onchange_signal(int fd)
{
	int ret, arg = 0;
	
	ret = ioctl(fd, ATC_TOD_CANCEL_ONCHANGE_SIG, &arg);
	if (ret < 0)
		return ret;
		
	close(fd);
	return 0;
}


// read the TZif2 file into buffer passed.
// return a pointer to the full POSIX TZ string within.
#ifndef TZNAME_MAX
#define TZNAME_MAX 6
#endif
#define TZ_BUFLEN   (2*TZNAME_MAX + 56)
static char *read_TZ_file(char *buf)
{
	int r;
	int fd;
	char *p = NULL;

	fd = open("/etc/localtime", O_RDONLY);
	if (fd >= 0) {
		r = read(fd, buf, TZ_BUFLEN);
		if ((r != TZ_BUFLEN) || (strncmp(buf, "TZif", 4) != 0)
			|| ((unsigned char)buf[4] < 2)
			|| (lseek(fd, -TZ_BUFLEN, SEEK_END) < 0)) {
			goto ERROR;
		}
		/* tzfile.h from tzcode database says about TZif2+ files:
		**
		** If tzh_version is '2' or greater, the above is followed by a second instance
		** of tzhead and a second instance of the data in which each coded transition
		** time uses 8 rather than 4 chars,
		** then a POSIX-TZ-environment-variable-style string for use in handling
		** instants after the last transition time stored in the file
		** (with nothing between the newlines if there is no POSIX representation for
		** such instants).
		*/
		r = read(fd, buf, TZ_BUFLEN);
		if ((r <= 0) || (buf[--r] != '\n'))
			goto ERROR;
		buf[r] = 0;
		while (r != 0) {
			if (buf[--r] == '\n') {
				p = buf + r + 1;
				break;
			}
		} /* else ('\n' not found): p remains NULL */
		close(fd);
	} else {
ERROR:
		p = NULL;
		close(fd);
	}

	return p;
}

typedef struct {
        long gmt_offset;
        long dst_offset;
        short day;                          /* for J or normal */
        short week;
        short month;
        short rule_type;                    /* J, M, \0 */
        char tzname[TZNAME_MAX+1];
} rule_struct;

/*
 * Extract offset from POSIX time zone string. 
 */
static char *getoffset(char *e, long *pn)
{
	const char *s = "\0\x19\x3c\x3c\x01";
	long n;
	int f;

	n = 0;
	f = -1;
	do {
		++s;
		if (__isdigit_char(*e)) {
			f = *e++ - '0';
		}
		if (__isdigit_char(*e)) {
			f = 10 * f + (*e++ - '0');
		}
		if (((unsigned int)f) >= *s) {
			return NULL;
		}
		n = (*s) * n + f;
		f = 0;
		if (*e == ':') {
			++e;
			--f;
		}
	} while (*s > 1);

	*pn = n;
	return e;
}

/*
 * Extract number from POSIX time zone string. 
 */
static char *getnumber(char *e, int *pn)
{
	int n, f;

	n = 3;
	f = 0;
	while (n && __isdigit_char(*e)) {
		f = 10 * f + (*e++ - '0');
		--n;
	}

	*pn = f;
	return (n == 3) ? NULL : e;
}

int get_dst_rules(rule_struct *rules) {
	char buf[TZ_BUFLEN];
	char *e, *s;
	long off = 0;
	short *p;
	int n, count, f;
	char c;

	if (rules == NULL)
		return -1;
	
	// Read tzif file
	e = read_TZ_file(buf);
	
	if (!e	|| !*e)	{	/* no TZfile (or bad TZfile) */
	 			/* or set to empty string. */
		printf("read_TZ_file\n");
		goto ILLEGAL;
	}

	if (*e == ':') {	/* Ignore leading ':'. */
		++e;
	}

//uClibc Method
	count = 0;
	rules[1].tzname[0] = 0;
LOOP:
	/* Get std or dst name. */
	c = 0;
	if (*e == '<') {
		++e;
		c = '>';
	}

	s = rules[count].tzname;
	n = 0;
	while (*e
	    && isascii(*e)		/* SUSv3 requires char in portable char set. */
	    && (isalpha(*e)
		|| (c && (isalnum(*e) || (*e == '+') || (*e == '-')))
	       )
	) {
		*s++ = *e++;
		if (++n > TZNAME_MAX) {
			printf("TZNAME_MAX\n");
			goto ILLEGAL;
		}
	}
	*s = 0;

	if ((n < 3)			/* Check for minimum length. */
	 || (c && (*e++ != c))	/* Match any quoting '<'. */
	) {
		printf("min len\n");
		goto ILLEGAL;
	}

	/* Get offset */
	s = (char *) e;
	if ((*e != '-') && (*e != '+')) {
		if (count && !__isdigit_char(*e)) {
			off -= 3600;		/* Default to 1 hour ahead of std. */
			goto SKIP_OFFSET;
		}
		--e;
	}

	++e;
	e = getoffset(e, &off);
	if (!e) {
		printf("getoffset\n");
		goto ILLEGAL;
	}

	if (*s == '-') {
		off = -off;				/* Save off in case needed for dst default. */
	}
SKIP_OFFSET:
	rules[count].gmt_offset = off;

	if (!count) {
		rules[1].gmt_offset = off; /* Shouldn't be needed... */
		if (*e) {
			++count;
			goto LOOP;
		}
	} else {					/* OK, we have dst, so get some rules. */
		count = 0;
		if (!*e) {				/* No rules so default to US 2007+ rules. */
			e = ",M3.2.0,M11.1.0";
		}

		do {
			if (*e++ != ',') {
				printf("first comma\n");
				goto ILLEGAL;
			}

			n = 365;
			s = (char *) "\x01.\x01\x05.\x01\x06\x00\x00\x00\x01\x00";
			c = *e++;
			if (c == 'M') {
				n = 12;
			} else if (c == 'J') {
				s += 8;
			} else {
				--e;
				c = 0;
				s += 6;
			}

			p = &rules[count].rule_type;
			*p = c;
			if (c != 'M') {
				p -= 2;
			}

			do {
				++s;
				e = getnumber(e, &f);
				if (!e
				 || ((unsigned int)(f - s[1]) > n)
				 || (*s && (*e++ != *s))
				) {
					printf("rule %d, s=%x e=%x\n", count, *s, *--e);
					goto ILLEGAL;
				}
				*--p = f;
				s += 2;
				n = *s;
			} while (n > 0);

			off = 2 * 60 * 60;	/* Default to 2:00:00 */
			if (*e == '/') {
				++e;
				e = getoffset(e, &off);
				if (!e) {
					printf("offset %d\n",count);
					goto ILLEGAL;
				}
			}
			rules[count].dst_offset = off;
		} while (++count < 2);

		if (*e) {
ILLEGAL:
			return -1;
		}
	}
	
	return 0;
}

int tod_get_dst_info(dst_info_t *dst_info)
{
	rule_struct new_rules[2];

	if ((dst_info == NULL)
		|| (get_dst_rules(new_rules) == -1)
		|| (new_rules[1].tzname[0] == 0)) {
		// Error getting DST rule info or no rule in place
		errno = EINVAL;
		return -1;
	}
		
	// Populate dst_info_t from POSIX rules, if any.
	// Rules are stored in new_rules[] array
	dst_info->type = 1; //generic style rules
	// DST Begin Rule
	dst_info->begin.generic.month = new_rules[0].month;
	if (new_rules[0].week == 5) {
		// Specify last occurence in month of dow
		dst_info->begin.generic.dom_type = 2;
		dst_info->begin.generic.generic_dom.reverse_occurrences_of_dow.dow = new_rules[0].day;
		dst_info->begin.generic.generic_dom.reverse_occurrences_of_dow.occur = 1;
		dst_info->begin.generic.generic_dom.reverse_occurrences_of_dow.on_before_dom = 0; //END_OF_MONTH
	} else {
		// Specify nth ocurence in month of dow
		dst_info->begin.generic.dom_type = 1;
		dst_info->begin.generic.generic_dom.forward_occurrences_of_dow.dow = new_rules[0].day;
		dst_info->begin.generic.generic_dom.forward_occurrences_of_dow.occur = new_rules[0].week;
		dst_info->begin.generic.generic_dom.forward_occurrences_of_dow.on_after_dom = 0; //BEGINNING_OF_MONTH
	}
	dst_info->begin.generic.secs_from_midnight_to_transition = new_rules[0].dst_offset;
	dst_info->begin.generic.seconds_to_adjust = new_rules[0].gmt_offset - new_rules[1].gmt_offset;
	
	// DST End Rule
	dst_info->end.generic.month = new_rules[1].month;
	if (new_rules[1].week == 5) {
		// Specify last occurence in month of dow
		dst_info->end.generic.dom_type = 2;
		dst_info->end.generic.generic_dom.reverse_occurrences_of_dow.dow = new_rules[1].day;
		dst_info->end.generic.generic_dom.reverse_occurrences_of_dow.occur = 1;
		dst_info->end.generic.generic_dom.reverse_occurrences_of_dow.on_before_dom = 0; //END_OF_MONTH
	} else {
		// Specify nth ocurence in month of dow
		dst_info->end.generic.dom_type = 1;
		dst_info->end.generic.generic_dom.forward_occurrences_of_dow.dow = new_rules[1].day;
		dst_info->end.generic.generic_dom.forward_occurrences_of_dow.occur = new_rules[1].week;
		dst_info->end.generic.generic_dom.forward_occurrences_of_dow.on_after_dom = 0; //BEGINNING_OF_MONTH
	}
	dst_info->end.generic.secs_from_midnight_to_transition = new_rules[1].dst_offset;
	dst_info->end.generic.seconds_to_adjust = new_rules[1].gmt_offset - new_rules[0].gmt_offset;
	
	return 0;
}

int tod_set_dst_info(const dst_info_t *dst_info)
{
	char buf[64+TZ_BUFLEN] = {'T','Z','i','f','2', 0};
	char *tzstr = &buf[54];
	int fd;
	int begin_month, begin_day, begin_occ;
	int end_month, end_day, end_occ;
	
	if (dst_info == NULL) {
		errno = EINVAL;
		return -1;
	}

	// Initially we convert to POSIX TZ string only.
	// Initially we support generic rules of type "nth weekday of month"
	// or "last weekday of month", which covers vast majority of cases.
	if (dst_info->type) {
		// generic rules defined
		// We need Month, DOW, occurence number
		begin_month = dst_info->begin.generic.month;
		if ((dst_info->begin.generic.dom_type == 1)
			&& (dst_info->begin.generic.generic_dom.forward_occurrences_of_dow.on_after_dom < 2)) {
			// nth occurence of day from beginning of month
			begin_day = dst_info->begin.generic.generic_dom.forward_occurrences_of_dow.dow;
			begin_occ = dst_info->begin.generic.generic_dom.forward_occurrences_of_dow.occur;
		} else if ((dst_info->begin.generic.dom_type == 2)
			&& (dst_info->begin.generic.generic_dom.reverse_occurrences_of_dow.on_before_dom == 0)
			&& (dst_info->begin.generic.generic_dom.reverse_occurrences_of_dow.occur == 1)) {
			// last occurence of day from end of month
			begin_day = dst_info->begin.generic.generic_dom.reverse_occurrences_of_dow.dow;
			begin_occ = 5;
		} else {
			//error can't represent?
			errno = EINVAL;
			return -1;
		}
		end_month = dst_info->end.generic.month;
		if ((dst_info->end.generic.dom_type == 1)
			&& (dst_info->end.generic.generic_dom.forward_occurrences_of_dow.on_after_dom < 2)) {
			// nth occurence of day from beginning of month
			end_day = dst_info->end.generic.generic_dom.forward_occurrences_of_dow.dow;
			end_occ = dst_info->end.generic.generic_dom.forward_occurrences_of_dow.occur;
		} else if ((dst_info->end.generic.dom_type == 2)
			&& (dst_info->end.generic.generic_dom.reverse_occurrences_of_dow.on_before_dom == 0)
			&& (dst_info->end.generic.generic_dom.reverse_occurrences_of_dow.occur == 1)) {
			// last occurence of day from end of month
			end_day = dst_info->end.generic.generic_dom.reverse_occurrences_of_dow.dow;
			end_occ = 5;
		} else {
			//error can't represent?
			errno = EINVAL;
			return -1;
		}
		// Fill std offset from "timezone" global
		tzstr += sprintf(tzstr, "\nATCST%2.2ld:%2.2ld:%2.2ld",
			timezone/3600, (timezone%3600)/60, (timezone%3600)%60);
		// Fill dst offset
		tzstr += sprintf(tzstr, "ATCDT%2.2ld:%2.2ld:%2.2ld",
			(timezone - dst_info->begin.generic.seconds_to_adjust)/3600,
			((timezone - dst_info->begin.generic.seconds_to_adjust)%3600)/60,
			((timezone - dst_info->begin.generic.seconds_to_adjust)%3600)%60);
		// Fill begin dst rule
		tzstr += sprintf(tzstr, ",M%d.%d.%d/%2.2d:%2.2d:%2.2d",
			begin_month,
			begin_occ,
			begin_day,
			dst_info->begin.generic.secs_from_midnight_to_transition/3600,
			(dst_info->begin.generic.secs_from_midnight_to_transition%3600)/60,
			(dst_info->begin.generic.secs_from_midnight_to_transition%3600)%60);
		// Fill end dst rule
		tzstr += sprintf(tzstr, ",M%d.%d.%d/%2.2d:%2.2d:%2.2d\n",
			end_month,
			end_occ,
			end_day,
			dst_info->end.generic.secs_from_midnight_to_transition/3600,
			(dst_info->end.generic.secs_from_midnight_to_transition%3600)/60,
			(dst_info->end.generic.secs_from_midnight_to_transition%3600)%60);
		// Write to temp file
		fd = open("/etc/localtime~", O_RDWR|O_CREAT|O_TRUNC);
		if (fd >= 0) {
			write(fd, buf, (tzstr-buf));
			fsync(fd);
			close(fd);
		} else {
			return -1;
		}
		// Rename to actual filename (or move to /usr/share/zoneinfo and link?)
		if (rename("/etc/localtime~", "/etc/localtime") == -1)
			return -1;
	} else {
		// absolute rules defined
		errno = EINVAL;
		return -1;
	}
	
	return 0;
}

int tod_get_dst_state(void)
{
	tzset();
	return (daylight?1:0);
}

int tod_set_dst_state(int state)
{
	// if we enable dst, where do we get the rules from?
	// if we disable dst, where do we store the rules?
	
	tzset();
	if (!state != !daylight) {
		// action required
		if (state) {
			// enable DST (what rule?)
		} else {
			// disable DST
			// rewrite tzfile with no rule
			// try moving the trailing '\n' delimiter and preserving any DST rule after it?
		}
	}
	
	return 0;
}


int tod_get(struct timeval *tv, int *tzsec_off, int *dst_off)
{
	struct tm local_time;
	struct timeval utc;
	
	gettimeofday(&utc, NULL);
	tzset();
	localtime_r(&utc.tv_sec, &local_time);
	if (tv != NULL) {
		tv->tv_sec = utc.tv_sec - timezone + ((daylight && local_time.tm_isdst)?3600:0);
                tv->tv_usec = utc.tv_usec;
	}
	if (tzsec_off != NULL) {
		*tzsec_off = -timezone;
	}
	if (dst_off != NULL) {
		/* Correct for all but the 360 residents of Lord Howe Island! */
		*dst_off = ((daylight && local_time.tm_isdst)?3600:0);
	}

	return 0;
}

int tod_set(const struct timeval *tv, const int *tzsec_off)
{
	struct stat sb;
	struct timeval tval;
        struct tm tm;
	rule_struct dst_rules[2];
	char buf[64+TZ_BUFLEN] = {'T','Z','i','f','2', 0};
	char *tzstr = &buf[54];
	int tzoff, dstoff;
	int fd;

	tzset();
	if ((tzsec_off != NULL) && (*tzsec_off != -timezone)) {
		// We must set the timezone offset but preserve any existing DST rule
		
		// Read existing DST Rule ?
		// is /etc/localtime present?
		if (stat("/etc/localtime", &sb) == 0) {
			if (get_dst_rules(dst_rules) == -1)
				return -1;
			// modify rule info and rewrite
			tzoff = -(*tzsec_off);
			dstoff = tzoff - (dst_rules[0].gmt_offset - dst_rules[1].gmt_offset);
			// Fill std offset
			tzstr += sprintf(tzstr, "\nATCST%2.2d:%2.2d:%2.2d",
				tzoff/3600, (tzoff%3600)/60, (tzoff%3600)%60);
			// Fill dst rules, if any
			if (!!dst_rules[1].tzname[0]) {
				// Fill dst offset
				tzstr += sprintf(tzstr, "ATCDT%2.2d:%2.2d:%2.2d",
					dstoff/3600, (dstoff%3600)/60, (dstoff%3600)%60);
				// Fill begin dst rule
				tzstr += sprintf(tzstr, ",M%d.%d.%d/%2.2ld:%2.2ld:%2.2ld",
					dst_rules[0].month,
					dst_rules[0].week,
					dst_rules[0].day,
					dst_rules[0].dst_offset/3600,
					(dst_rules[0].dst_offset%3600)/60,
					(dst_rules[0].dst_offset%3600)%60);
				// Fill end dst rule
				tzstr += sprintf(tzstr, ",M%d.%d.%d/%2.2ld:%2.2ld:%2.2ld",
					dst_rules[1].month,
					dst_rules[1].week,
					dst_rules[1].day,
					dst_rules[1].dst_offset/3600,
					(dst_rules[1].dst_offset%3600)/60,
					(dst_rules[1].dst_offset%3600)%60);
			}
			*tzstr++ = '\n';
			// Write to temp file
			fd = open("/etc/localtime~", O_RDWR|O_CREAT|O_TRUNC);
			if (fd >= 0) {
				write(fd, buf, (tzstr-buf));
				fsync(fd);
				close(fd);
			} else {
				return -1;
			}
			// Rename to actual filename (or move to /usr/share/zoneinfo and link?)
			if (rename("/etc/localtime~", "/etc/localtime") == -1)
				return -1;			
		}
		// Update globals with new timezone
		tzset();
	}
	
	if (tv != NULL) {
		tval = *tv;
                if (tzsec_off == NULL)
                        tzset();
                gmtime_r(&tval.tv_sec, &tm);
                tm.tm_isdst = -1;
                if (mktime(&tm) == -1)
                        return -1;
		tval.tv_sec += timezone - ((daylight && tm.tm_isdst)?3600:0);
		settimeofday(&tval, NULL);
	}
	return 0;
}



