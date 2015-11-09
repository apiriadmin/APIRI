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

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <errno.h>
#include <poll.h>

#include "fpui.h"
#include "front_panel.h"

#define FRONT_PANEL_DEV "/dev/fpi"
#define FRONT_PANEL_AUX_DEV "/dev/aux"
#define FRONT_PANEL_SYSCONFIG_DEV "/dev/sci"
#define ESC "\x1b"
#define LF	'\n'
#define CR	'\r'

/*
	this library represents wrappers and convenience functions for standard
	unix calls and escape sequences as defined by the ATC 5.0 Specification
*/


/*
	General Operators
*/
static char version_buf[80];

char *fpui_apiver( fpui_handle fd, int type )
{
	switch( type ) {
		case 1:
			return( "Intelight, 1.0, 2.17" );
		case 2:
			if( ioctl( fd, FP_IOC_VERSION, version_buf ) < 0 ) {
				return( NULL );
			} else {
				return( version_buf );
			}
		default:
			break;
	}
	return( NULL );
}


int fpui_open( int flags, const char * regname )
{
	int fd    = 0;
	int errcd = 0;

	if( (fd = open( FRONT_PANEL_DEV, flags )) < 0 ) {
		return( fd );
	}

	if( (errcd = ioctl( fd, FP_IOC_REGISTER, regname )) < 0 ) {
		close( fd );
		return( errcd );
	}

	return( fd );
}


int fpui_close( fpui_handle fd )
{
	return( close( fd ) );
}

int fpui_open_config_window( int flags )
{
	return (open(FRONT_PANEL_SYSCONFIG_DEV, flags));
}


int fpui_close_config_window( fpui_handle fd )
{
	return( close( fd ) );
}

int fpui_get_window_size( fpui_handle fd, int *rows, int *columns )
{
	int  err = -1;
	char sbuf[16];
	char panel_type;

	//Query panel type: A (4 lines); B (8 lines); D (16 lines)
	if( (err = fpui_write_string( fd, ESC "[c" )) < 0 ) {
		return( err );
	}

	if( (err = fpui_read( fd, sbuf, sizeof( sbuf ) )) < 0 ) {
		return( err );
	}

	sscanf( sbuf, ESC "[%cR", &panel_type );
	*columns = 40;
	if (panel_type == 'A')
		*rows = 4;
	else if (panel_type == 'B')
		*rows = 8;
	else if (panel_type == 'D')
		*rows = 16;
	else
		return -1;
		
	return( 0 );
}


int fpui_get_focus( fpui_handle fd )
{
	int  err;
	char sbuf[16];
	char focus;

	if( (err = fpui_write_string( fd, ESC "[Fn" )) < 0 ) {
		return( err );
	}

	if( (err = fpui_read( fd, sbuf, sizeof( sbuf ) )) < 0 ) {
		return( err );
	}

	sscanf( sbuf, ESC "[%cR", &focus );
	if (focus == 'h')
		return 1;
	if (focus == 'l')
		return 0;

	return -1;
}


int fpui_clear( fpui_handle fd )
{
	int err = fpui_write_string( fd, ESC "[2J" );
	if (err < 0)
		return err;
	return 0;
}


int fpui_refresh( int fd )
{
	// we need an ioctl call via the dev/fpi driver
	return (ioctl(fd, FP_IOC_REFRESH));
}


int fpui_set_emergency( fpui_handle fd, bool state )
{
	return( ioctl( fd, FP_IOC_EMERGENCY, state ) < 0 );
}





/*
	Attribute Operators
*/
int fpui_set_window_attr( fpui_handle fd, int attr )
{
	int err = 0;
	fpui_attr_t new_attr = (fpui_attr_t)attr;
	fpui_attr_t old_attr = (fpui_attr_t)fpui_get_window_attr( fd );

	if (old_attr.errcode == -1)
		return -1;
	if ( new_attr.auto_wrap != old_attr.auto_wrap )
		err = fpui_set_auto_wrap( fd, new_attr.auto_wrap );
	if ( (err == 0) && (new_attr.auto_scroll != old_attr.auto_scroll) )
		err = fpui_set_auto_scroll( fd, new_attr.auto_scroll );
	if ( (err == 0) && (new_attr.auto_repeat != old_attr.auto_repeat) )
		err = fpui_set_auto_repeat( fd, new_attr.auto_repeat );
	if ( (err == 0) && (new_attr.backlight != old_attr.backlight) )
		err = fpui_set_backlight( fd, new_attr.backlight );
	if ( (err == 0) && (new_attr.backlight_timeout != old_attr.backlight_timeout) )
		err = fpui_set_backlight_timeout( fd, new_attr.backlight_timeout*10 );

	return err;
}

int fpui_get_window_attr( fpui_handle fd )
{
	char buf[24];
	char p1, p2, p3, p4, p6;
	int p5;
	fpui_attr_t attr;

	if( fpui_write_string( fd, ESC "[Bn" ) < 0 ) {
		return( errno );
	}

	if( fpui_read( fd, buf, sizeof( buf ) ) < 0 ) {
		return( errno );
	}

	sscanf( buf, ESC "[%c;%c;%c;%c;%d;%cR", &p1, &p2, &p3, &p4, &p5, &p6 );
	attr.auto_wrap = (p1 == 'h')?1:0;
	attr.auto_scroll = (p2 == 'h')?1:0;
	attr.auto_repeat = (p3 == 'h')?1:0;
	attr.backlight = (p4 == 'h')?1:0;
	attr.backlight_timeout = p5;

	return (attr.errcode);

}

int fpui_get_attributes( int fd, int index )
{
	fpui_attr_t attr;

	attr = (fpui_attr_t) fpui_get_window_attr(fd);
	if (attr.errcode < 0)
		return -1;

	switch( index ) {
		case 1:
			return( attr.auto_wrap );
		case 2:
			return( attr.auto_scroll );
		case 3:
			return( attr.auto_repeat );
		case 4:
			return( attr.backlight );
		case 5:
			return( attr.backlight_timeout );
		/*	the Aux switch is handled separately and should not be available through this library.*/
		case 6:
			return( attr.aux_switch );
	}
	return( 0 );
}


int fpui_set_character_blink( fpui_handle fd, bool state )
{
	if( state ) {
		return( fpui_write_string( fd, ESC "[25h" ) );
	} else {
		return( fpui_write_string( fd, ESC "[25l" ) );
	}
	return( 0 );
}


bool  fpui_get_character_blink( fpui_handle fd )
{
	unsigned int mask;

	if( ioctl( fd, FP_IOC_ATTRIBUTES, &mask ) < 0 ) {
		return( -1 );
	}

	return( (mask & (1 << BIT_CHARACTER_BLINK)) != 0 );
}


int fpui_set_backlight( fpui_handle fd, bool state )
{
	if( state ) {
		return( fpui_write_string( fd, ESC "[<5h" ) );
	} else {
		return( fpui_write_string( fd, ESC "[<5l" ) );
	}
	return( 0 );
}

bool  fpui_get_backlight( fpui_handle fd )
{
	return( fpui_get_attributes( fd, 4 ) );
}

int fpui_set_backlight_timeout( fpui_handle fd, int timeout )
{
	char sbuf[16];

	sprintf( sbuf, ESC "[<%dS", timeout/10 );
	return( fpui_write_string( fd, sbuf ) );
}

int fpui_set_cursor_blink( fpui_handle fd, bool state )
{
	if( state ) {
		return( fpui_write_string( fd, ESC "[33h" ) );
	} else {
		return( fpui_write_string( fd, ESC "[33l" ) );
	}
	return( 0 );
}


bool  fpui_get_cursor_blink( fpui_handle fd )
{
	unsigned int mask;

	if( ioctl( fd, FP_IOC_ATTRIBUTES, &mask ) < 0 ) {
		return( -1 );
	}

	return( (mask & (1 << BIT_CURSOR_BLINK)) != 0 );
}


int fpui_set_reverse_video( fpui_handle fd, bool state )
{
	if( state ) {
		return( fpui_write_string( fd, ESC "[27h" ) );
	} else {
		return( fpui_write_string( fd, ESC "[27l" ) );
	}
	return( 0 );
}


bool  fpui_get_reverse_video( fpui_handle fd )
{
	unsigned int mask;

	if( ioctl( fd, FP_IOC_ATTRIBUTES, &mask ) < 0 ) {
		return( -1 );
	}

	return( (mask & (1 << BIT_CURSOR_BLINK)) != 0 );
}


int fpui_set_underline( fpui_handle fd, bool state )
{
	if( state ) {
		return( fpui_write_string( fd, ESC "[24h" ) );
	} else {
		return( fpui_write_string( fd, ESC "[24l" ) );
	}
	return( 0 );
}


bool  fpui_get_underline( fpui_handle fd )
{
	unsigned int mask;

	if( ioctl( fd, FP_IOC_ATTRIBUTES, &mask ) < 0 ) {
		return( -1 );
	}

	return( (mask & (1 << BIT_UNDERLINE)) != 0 );
}


int fpui_set_auto_wrap( fpui_handle fd, bool state )
{
	if( state ) {
		return( fpui_write_string( fd, ESC "[?7h" ) );
	} else {
		return( fpui_write_string( fd, ESC "[?7l" ) );
	}
	return( 0 );
}


bool  fpui_get_auto_wrap( fpui_handle fd )
{
	return( fpui_get_attributes( fd, 1 ) );
}


int fpui_set_auto_repeat( fpui_handle fd, bool state )
{
	if( state ) {
		return( fpui_write_string( fd, ESC "[?8h" ) );
	} else {
		return( fpui_write_string( fd, ESC "[?8l" ) );
	}
	return( 0 );
}


bool  fpui_get_auto_repeat( fpui_handle fd )
{
	return( fpui_get_attributes( fd, 3 ) );
}


int fpui_set_cursor( fpui_handle fd, bool state )
{
	if( state ) {
		return( fpui_write_string( fd, ESC "[?25h" ) );
	} else {
		return( fpui_write_string( fd, ESC "[?25l" ) );
	}
	return( 0 );
}


bool  fpui_get_cursor( fpui_handle fd )
{
	unsigned int mask;

	if( ioctl( fd, FP_IOC_ATTRIBUTES, &mask ) < 0 ) {
		return( -1 );
	}

	return( (mask & (1 << BIT_CURSOR)) != 0 );
}


int fpui_set_auto_scroll( fpui_handle fd, bool state )
{
	if( state ) {
		return( fpui_write_string( fd, ESC "[?47h" ) );
	} else {
		return( fpui_write_string( fd, ESC "[?47l" ) );
	}
	return( 0 );
}


bool  fpui_get_auto_scroll( fpui_handle fd )
{
	return( fpui_get_attributes( fd, 2 ) );
}


int fpui_reset_all_attributes( fpui_handle fd )
{
	return( fpui_write_string( fd, ESC "[0m" ) );
}




/*
	Read Operators
*/
int fpui_poll( fpui_handle fd, int flags )
{
	int ret;
	struct pollfd fds;
	int timeout = -1;

	fds.fd = fd;
	fds.events = POLLIN;

	if (flags & O_NONBLOCK)
		timeout = 0;
		
	ret = poll(&fds, 1, timeout);
	if (ret > 1)
		ret = 1;

	return ret;
}
 
ssize_t fpui_read( fpui_handle fd, char * buffer, int bufsize )
{
	return( read( fd, buffer, bufsize ) );
}


char fpui_read_char( fpui_handle fd )
{
	char ch;

	if (read(fd, &ch, 1) != 1)
		return -1;
	else
		return ch;
}


ssize_t fpui_read_string( fpui_handle fd, char *string, int stringsize )
{
	int i = 0;

	for( i = 0; i < (stringsize-1); i++ ) {
		read( fd, &string[i], 1 );
		if( string[i] == '\0' ) {
			break;
		}
	}
	string[i] = '\0';
	return( i );
}


/*
	Write Operators
*/
ssize_t fpui_write( fpui_handle fd, char *buffer, int buflen )
{
	return( write(fd, buffer, buflen) );
}


ssize_t fpui_write_char( fpui_handle fd, char ch )
{
	return( write( fd, &ch, 1 ) );
}


ssize_t fpui_write_string( fpui_handle fd, char *string )
{
	return( fpui_write( fd, string, strlen(string) ) );
}


ssize_t fpui_write_at( fpui_handle fd, char *buffer, int buflen, int row, int column )
{
	int errcd;

	if( (errcd = fpui_set_cursor_pos( fd, row, column )) < 0 ) {
		return( errcd );
	}

	return( fpui_write( fd, buffer, buflen ) );
}


ssize_t fpui_write_char_at( fpui_handle fd, char ch, int row, int column )
{
	int errcd;

	if( (errcd = fpui_set_cursor_pos( fd, row, column )) < 0 ) {
		return( errcd );
	}

	return( write( fd, &ch, 1 ) );
}


ssize_t fpui_write_string_at( fpui_handle fd, char *string, int row, int column )
{
	int errcd;

	if( (errcd = fpui_set_cursor_pos( fd, row, column )) < 0 ) {
		return( errcd );
	}

	return( fpui_write( fd, string, strlen(string) ) );
}


/*
	Cursor Operators
*/
int fpui_get_cursor_pos( fpui_handle fd,  int * row, int * column )
{
	int  errcd;
	char sbuf[16];

	if( (errcd = fpui_write_string( fd, ESC "[6n" )) < 0 ) {
		return( errcd );
	}

	if( (errcd = fpui_read( fd, sbuf, sizeof( sbuf ) )) < 0 ) {
		return( errcd );
	}

	sscanf( sbuf, ESC "[%d;%dR", row, column );
	return( 0 );
}


int fpui_set_cursor_pos( fpui_handle fd,  int row, int column )
{
	char sbuf[16];
	sprintf( sbuf, ESC "[%d;%df", row, column );
	return( fpui_write_string( fd, sbuf ) );
}


int fpui_home( fpui_handle fd )
{
	return( fpui_write_string( fd, ESC "[H" ) );
}


int fpui_set_tab( fpui_handle fd )
{
	return( fpui_write_string( fd, ESC "H" ) );
}


int fpui_clear_tab( fpui_handle fd, int type )
{
	char buf[8];
	if( (type >= 0) && (type <= 3) ) {
		sprintf( buf, ESC "[%dg", type );
		return( fpui_write_string( fd, buf ) );
	}
	errno = EINVAL;
	return( -1 );
}




/*
	Special Character Operators
*/
int fpui_compose_special_char( fpui_handle fd, int index, unsigned char *buffer )
{
	int i;
	int len = 0;
	char buf[40];

	len += sprintf( &buf[len], ESC "P%d[", index );
	for( i = 0; i < 8; i++ ) {
		len += sprintf( &buf[len], "%d;", buffer[i] );
	}
	buf[len-1] = 'f';

	return( fpui_write( fd, buf, len ) );
}


int fpui_display_special_char( fpui_handle fd, int index )
{
	int len = 0;
	char buf[40];

	len = sprintf( buf, ESC "[<%dV", index );
	return( fpui_write( fd, buf, len ) );
}


/*
	LED Operators
*/
/*int fpui_set_led( int fd, int state )
{
}


int fpui_get_led( int fd )
{
	return( 0 );
}
*/

/*
	AUX Switch Operators
*/
fpui_aux_handle fpui_open_aux_switch( void )
{
	int fd = 0;

	fd = open(FRONT_PANEL_AUX_DEV, O_RDONLY);
	return( fd );
}
int fpui_close_aux_switch( fpui_aux_handle fd )
{
	return close(fd);
}
int fpui_read_aux_switch( fpui_aux_handle fd )
{
	int  err;
	char sbuf[16];
	char auxsw;

	if( (err = fpui_write_string( fd, ESC "[An" )) < 0 ) {
		return( err );
	}

	if( (err = fpui_read( fd, sbuf, sizeof( sbuf ) )) < 0 ) {
		return( err );
	}

	sscanf( sbuf, ESC "[%cR", &auxsw );
	if (auxsw == 'h')
		return 1;
	if (auxsw == 'l')
		return 0;

	return -1;
}


/*
	Key Mapping Operators
*/

int fpui_set_keymap( fpui_handle fd, char key, char *seq )
{
	keymap_t km;

	km.cmd = CHAR_DC1;				// keymap set command
	km.key = key;					// returned key value
	strncpy( km.seq, seq, 6 );			// source escape sequence
	km.seq[6] = '\0';

	return( write( fd, &km, sizeof( km ) ) );	// send the command
}


int fpui_get_keymap( fpui_handle fd, char key, char *seq, int size )
{
	int n;
	keymap_t km;

	km.cmd = CHAR_DC2;				// keymap get command
	km.key = key;					// desired key value

	if( (n = write( fd, &km, 2 )) < 0 ) {		// send the inquiry
		return( n );
	}

	memset( &km, 0, sizeof( km ) );			// clear the buffer for the response

	if( (n = read(fd, &km, sizeof( km ))) >= 0 ) {	// read the response
		memmove(seq, km.seq, (size<7)?size:7);	// copy the escape sequence to the users buffer
	}

	return( n );
}


int fpui_del_keymap( fpui_handle fd, char key )
{
	keymap_t km;

	km.cmd = CHAR_DC3;				// keymap delete command
	km.key = key;					// desired key value

	return( write( fd, &km, 2 ) );			// send the command
}


int fpui_reset_keymap( fpui_handle fd, int type )
{
	keymap_t km;

	if (type<0 || type>1)
		return -1;
		
	km.cmd = CHAR_DC4;				// keymap reset command
	km.key = type?'1':'0';

	return( write( fd, &km, 2 ) );			// send the command
}
