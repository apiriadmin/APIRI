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

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <errno.h>
#include <string.h>


int xprintf(int fd, const char * fmt, ...)
{
	int        n; 
	int        size = 41; // the default number of columns
	char       *s;
	char       *sx;
	va_list    ap;

	if ((s = calloc( 1, size )) == NULL) {
		return -1;
	}

	for(;;) {
		// Try to print in the allocated space.
		va_start(ap, fmt);
		n = vsnprintf(s, size, fmt, ap);
		va_end(ap);

		// If that worked, send the message
		if ((n > -1) && (n < size)) {
			n = write(fd, s, n);
			free(s);
			return n;
		}

		// Else try again with more space.
		if (n > -1) {				// glibc 2.1
			size = n + 1;			// precisely what is needed
		} else {       				// glibc 2.0
			size *= 2;			// twice the old size
		}

		if ((sx = realloc(s, size)) == NULL) {	// request a new larger allocation
			free(s);			// allocation failed, free original allocation
			return -1;			// return an error
		} else {
			s = sx;				// allocation succeeded, take new reference
		}
	}
}

