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
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <signal.h>
#include <errno.h>
#include <string.h>

#include <front_panel.h>

#undef DEBUG

#ifdef DEBUG
#define DBG(x...)           printf(x);

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
	} else if( slot == SC_DEV ) {
		return( "SC_DEV" );
	} else if( slot == AUX_DEV ) {
		return( "AUX_DEV" );
	} else if( slot == FPM_DEV ) {
		return( "FPM_DEV" );
	}
	return( "Overflow" );
}
#else
#define DBG(x...)           do{}while(0)
#endif

extern void set_focus( int );
extern void tohex( char * );
extern void tohexn( char *, int );
extern void virtual_terminal( int, char * );
extern void create_virtual_terminal( int );
extern void destroy_virtual_terminal( int );
extern void refresh_virtual_terminal( int );
extern int  is_active( int );


read_packet *make_packet( int command, int from, int to, char *s, char *t )
{
	int slen = 0, tlen = 0;
	
	if (s != NULL)
		slen = strlen(s);
		
	if (t != NULL)
		tlen = strlen(t);
	
	read_packet *rp = (read_packet *)calloc( 1, slen + tlen + sizeof(read_packet) );
	if( rp == NULL ) return( NULL );
	rp->command = command;
	rp->from    = from;
	rp->to      = to;
	rp->size    = slen + tlen;
	rp->raw_offset = slen;
	if (slen)
		memcpy( (char *)rp->data, s, slen );
	if (tlen)
		memcpy( (char *)&rp->data[slen], t, tlen );

	return( rp );
}

void free_packet( read_packet *rp )
{
    return free(rp);
}

int fpm = 0;

void routing( int fd )
{
	unsigned int i, *p_i;
	char buf[1024];
	read_packet *rp = (read_packet *)buf;
	fpm              = fd;
#ifdef DEBUG
	char *command_names[] = COMMAND_NAMES;
#endif

	DBG( "%s: Starting\n", __func__ );

	for( ;; ) {
		memset( buf, 0, sizeof( buf ) );
		if( (i = read( fpm, buf, sizeof( buf ) ) ) < 0 ) {
		        fprintf( stderr, "%s: Read error - %s - Exiting\n", __func__, strerror( errno ) );
			exit( 98 );
		} else if( i == 0 ) {
			DBG("%s: Zero length read\n", __func__ );
		    	continue;
		}
		
		/*
		*/
#ifdef DEBUG
		DBG("\nRouting:\n");
		DBG("command = %s (%d)\n",  command_names[rp->command], rp->command );
		DBG("from    = %s\n",  slot_to_string(rp->from) );
		DBG("to      = %s\n",  slot_to_string(rp->to) );
		DBG("size    = %ld\n", (long)rp->size );
		DBG("data    = \n" );
		tohexn( (char *)rp->data, (int)rp->size );
#endif
		switch( rp->command ) {
			case NOOP:
				DBG("%s: NOOP from %s to %s\n", __func__, slot_to_string(rp->from), slot_to_string(rp->to) );
				break;
			case DATA:
				DBG("%s: DATA from %s to %s (size=%ld)\n", __func__, slot_to_string(rp->from), slot_to_string(rp->to), (long)rp->size );
				virtual_terminal( rp->from, (char *)rp->data );		// just send the payload data to the virtual terminal
				break;
			case CREATE:
				DBG("%s: CREATE from %s to %s\n", __func__, slot_to_string(rp->from), slot_to_string(rp->to) );
				create_virtual_terminal( rp->from );		// create the virtual terminal
				break;
			case REGISTER:
				DBG("%s: REGISTER from %s to %s (size=%ld)\n", __func__, slot_to_string(rp->from), slot_to_string(rp->to), (long)rp->size );
				break;
			case DESTROY:
				DBG("%s: DESTROY from %s to %s\n", __func__, slot_to_string(rp->from), slot_to_string(rp->to) );
				destroy_virtual_terminal( rp->from );		// destroy the virtual terminal
				break;
			case FOCUS:
				p_i = (unsigned int *)rp->data;
				i = *p_i;
				DBG("%s: FOCUS from %s to %s request to set %d\n", __func__, slot_to_string(rp->from), slot_to_string(rp->to), i );
				if( is_active( i ) ) {
					set_focus( i );

					// 3.3.1.2-aa
					// as part of this requirement, each time focus changes, 
					// a message must be sent back to the driver so it can store
					// the index of the curremt application in focus.
					/*
					read_packet * rp = make_packet( FOCUS, FPM_DEV, i, NULL, 0 );

					if( write( fpm, rp, sizeof( read_packet ) ) < 0 ) {
						printf("%s: Write error - %s\n", __func__, strerror( errno ) );
					}
					*/
				} else {
					fprintf( stderr, "%s: Focus not active or out of range (%d)\n", __func__, i );
				}
				break;
			case ATTRIBUTES:
				// int attr = get_attributes(display)
				// routing_return(to, &attr, 0)
				break;
			case REFRESH:
				DBG("%s: REFRESH from %s to %s (size=%ld)\n", __func__, slot_to_string(rp->from), slot_to_string(rp->to), (long)rp->size );
				refresh_virtual_terminal( rp->from );
				break;
			case EMERGENCY:
				p_i = (unsigned int *)rp->data;
				i = *p_i;
				DBG("%s: EMERGENCY from %s to %s (size=%ld)\n", __func__, slot_to_string(rp->from), slot_to_string(rp->to), (long)rp->size );
				set_emergency(rp->from, i);
				break;
			default:
				DBG("%s: Undefined from %s to %s\n", __func__, slot_to_string(rp->from), slot_to_string(rp->to) );
				break;
		}
	}
}


// this routine handles returning responses from the virtual terminals to the
// driver for sorting and queueing. The variable 's' represents the mapped
// response while 't' represents the raw response.
void routing_return( int target, char *s, char *t )
{
	read_packet *rp = NULL;
	
	if ((s == NULL) && (t == NULL))
		return;
	
	rp = make_packet( DATA, FPM_DEV, target, s, t );

	DBG("%s: write combined mapped+raw response (%d bytes)\n", __func__, rp->size);
	if( write( fpm, rp, sizeof(read_packet) + rp->size ) < 0 ) {
		fprintf(stderr, "%s: Write error - %s\n", __func__, strerror( errno ) );
	}
	free_packet(rp);

	DBG("%s: write to fpm\n", __func__ );
}

void routing_send_signal( int to, int sig )
{
	read_packet *rp = make_packet( sig, FPM_DEV, to, NULL, NULL );

	if( write( fpm, rp, sizeof( read_packet ) ) < 0 ) {
		fprintf(stderr, "%s: Write error - %s\n", __func__, strerror( errno ) );
	}
    free_packet(rp);
}


