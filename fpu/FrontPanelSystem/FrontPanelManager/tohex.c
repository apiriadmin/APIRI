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
#include <string.h>

void tohex( unsigned char * s )
{
	int i = 0;
	int len = strlen((char *)s) + 1;

	printf("\t%4.4d: ", i );
	for( i = 0; i < len; i++ ) {
		printf("0x%2.2x ", s[i] );
		if( (i % 16) == 15 ) {
			printf("\n\t%4.4d: ", i+1 );
		}
	}

	if( (i % 16) != 15 ) {
		printf("\n");
	}
}

void tohexn( unsigned char * s, int len )
{
	int i = 0;

	printf("\t%4.4d: ", i );
	for( i = 0; i < len; i++ ) {
		printf("0x%2.2x ", s[i] );
		if( (i % 16) == 15 ) {
			printf("\n\t%4.4d: ", i+1 );
		}
	}

	if( (i % 16) != 15 ) {
		printf("\n");
	}
}
