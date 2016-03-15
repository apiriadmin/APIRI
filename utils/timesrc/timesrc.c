/*
 * Copyright 2014-2016 AASHTO/ITE/NEMA.
 * American Association of State Highway and Transportation Officials,
 * Institute of Transportation Engineers and
 * National Electrical Manufacturers Association.
 *  
 * This file is part of the Advanced Transportation Controller (ATC)
 * Application Programming Interface Reference Implementation (APIRI).
 *  
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *  
 * The APIRI is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *  
 * You should have received a copy of the GNU Lesser General Public
 * License along with the APIRI.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <string.h>
#include <strings.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <linux/ioctl.h>
#include <atc.h>

static const char* NTPCONFIG_FILE = "/etc/ntp.conf";
static const char* NTPD_FILE = "/etc/init.d/S49ntp";
static const char* NTPGPS_FILE = "/etc/default/ntpd";

struct timesrc_table {
	const char	*name;
	int		value;
};

static const struct timesrc_table timesrcs[] =
{
	{ "LINESYNC",		ATC_TIMESRC_LINESYNC },
	{ "RTC_SQUARE_WAVE",	ATC_TIMESRC_RTCSQWR },
	{ "CRYSTAL",		ATC_TIMESRC_CRYSTAL },
	{ "EXTERNAL1",		ATC_TIMESRC_EXTERNAL1 },
	{ "EXTERNAL2",		ATC_TIMESRC_EXTERNAL2 },
	{ NULL, 0 }
};

static int timesrc_to_enum(const struct timesrc_table *tab, const char *str)
{
    const struct timesrc_table *t;

    for (t = tab; t && t->name; t++)
	if (!strcasecmp(t->name, str))
	    return t->value;
    return -1;
}

static const char *enum_to_timesrc(const struct timesrc_table *tab, const int val)
{
    const struct timesrc_table *t;

    for (t = tab; t && t->name; t++)
	if (t->value == val)
	    return t->name;
    return NULL;
}

static char *progname;
static char *version = "1.0.0";

static void usage(void) {
    fprintf(stderr, "%s version %s - Copyright (C) 2014-2016 AASHTO/ITE/NEMA\n", progname, version);
    fprintf(stderr, "\n");
    fprintf(stderr, "usage:   %s - print current time source\n", progname);
    fprintf(stderr, "         %s <name> - set current time source [LINESYNC/RTC_SQUARE_WAVE/CRYSTAL]\n", progname);
    fprintf(stderr, "         %s EXTERNAL1 <port> - set GPS time source for serial port\n", progname);
    fprintf(stderr, "         %s EXTERNAL2 <hostname> - set NTP time source\n", progname);

    exit(1);
}

int main(int argc, char **argv)
{
	int fd, timesrc_cur, timesrc_new, portNumber = 1;
	bool set_timesrc = false;
	char command[80];

	if ((progname = strrchr(argv[0], '/')) != NULL)
		progname++;
	else
		progname = argv[0];

	if (argc > 3) {
		printf("Too many arguments\n");
		usage();
		goto exit;
	}
	if (argc > 1) {
		if ((timesrc_new = timesrc_to_enum(timesrcs, argv[1])) < 0) {
			printf("Unrecognized timesrc %s\n", argv[1]);
			usage();
			goto exit;
		}
		set_timesrc = true;
	}

	if ((fd = open("/dev/tod", O_RDWR)) == -1) {
		perror("Cannot open /dev/tod");
		goto exit;
	} else if ((timesrc_cur = ioctl(fd, ATC_TOD_GET_TIMESRC)) == -1) {
		perror("Cannot get current timesrc");
		goto close_exit;
	} else {
		printf("Current timesrc is %s(%d)\n", enum_to_timesrc(timesrcs, timesrc_cur), timesrc_cur);
		if (set_timesrc) {
			if (timesrc_cur == ATC_TIMESRC_EXTERNAL1) {
				// remove any entries in NTPGPS file for serial ports
				sprintf(command, "sed -i '\\_/dev/sp_ d' %s", NTPGPS_FILE);
				system(command);
				// remove ntp gps device link
				sprintf(command, "if readlink /dev/gps0 | grep -q sp; then rm /dev/gps0; fi");
				system(command);
			} else if (timesrc_cur == ATC_TIMESRC_EXTERNAL2) {
				// delete any server entries except where the first octet is 127
				sprintf(command, "sed -ri '/server ([^1]|.[^2]|..[^7]).*/ d' %s", NTPCONFIG_FILE);
				system(command);
			}
			if (timesrc_new == ATC_TIMESRC_EXTERNAL1) {
				if (argc > 2) {
					portNumber = atoi(argv[2]);
					if ((portNumber < 1) || (portNumber == 7) || (portNumber > 8)) {
						printf("Invalid serial port for timesrc %s (%d)\n",
							enum_to_timesrc(timesrcs, timesrc_new), portNumber);
						goto close_exit;
					}
				}
						
				// add ntp gps device links
				sprintf(command, "sed -i '$ a ln -s /dev/sp%d /dev/gps0' %s",
					portNumber, NTPGPS_FILE);
				system(command);
			} else if (timesrc_new == ATC_TIMESRC_EXTERNAL2) {
				// stop the ntpd daemon
				sprintf(command, "%s stop", NTPD_FILE);
				system(command);
				char ntp_peer[80] = "us.pool.ntp.org";
				if (argc > 2) {
					// TBD: allow multiple peer entries 
					// TBD: also validate new value(s)
					strncpy(ntp_peer, argv[2], 80);
				}
				// add new server entry on line 1
				sprintf(command, "sed -i '1 i server %s iburst' %s", ntp_peer, NTPCONFIG_FILE);
				system(command);
				// get the system clock close with a quick call of ntpdate
				//sprintf(command, "/usr/bin/ntpdate -p 2 -t 3 -b %s", ntp_peer);
				//system(command);
			} 
			// restart the ntp daemon
			sprintf(command, "%s restart", NTPD_FILE);
			system(command);
			
			printf("setting timesrc to %s(%d)\n", enum_to_timesrc(timesrcs, timesrc_new), timesrc_new);
			if (ioctl(fd, ATC_TOD_SET_TIMESRC, &timesrc_new) == -1) {
				perror("Cannot set driver timesrc");
				goto close_exit;
			} else {
				printf("timesrc set to %s\n", enum_to_timesrc(timesrcs, timesrc_new));
				// set u-boot parameter "timesrc" to make change persistent
				// (check for /etc/fw_env.config file)
				//sprintf(command, "fw_setenv timesrc %s", enum_to_timesrc(timesrcs, timesrc_new));
				//system(command);
				goto close_exit;
			}
		}
	}

close_exit:
	close(fd);
exit:
	exit(0);
}

