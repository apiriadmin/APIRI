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

#ifndef _FPUI_H_
#define _FPUI_H_
#include <unistd.h>
#include <stdbool.h>

#define CHAR_DC1	0x11
#define CHAR_DC2	0x12
#define CHAR_DC3	0x13
#define CHAR_DC4	0x14

#if !(__bool_true_false_are_defined)
typedef enum { false=(0!=0), true=(0==0) } bool;
#endif

typedef union {
	int		errcode;
	unsigned int	auto_wrap:1,
			auto_scroll:1,
			auto_repeat:1,
			backlight:1,
			cursor_on:1,
			cursor_blink:1,
			char_blink:1,
			aux_switch:1,
			backlight_timeout:8,
			char_reverse:1,
			char_underline:1,
			:14;
} fpui_attr_t;

/*
	General Operators
*/
typedef	int	fpui_handle;
typedef int	fpui_aux_handle;

char *fpui_apiver( fpui_handle fd, int type );
fpui_handle fpui_open( int flags, const char * regname );
int fpui_open_config_window( int flags );
int fpui_close( fpui_handle fd );
int fpui_close_config_window( fpui_handle fd );
int fpui_panel_present( fpui_handle fd );
int fpui_get_window_size( fpui_handle fd, int * rows, int * columns );
int fpui_get_focus( fpui_handle fd );
int fpui_clear( fpui_handle fd );
int fpui_refresh( fpui_handle fd );
int fpui_set_emergency( fpui_handle fd, bool state ) ;
/*
	Attribute Operators
*/
int fpui_set_window_attr( fpui_handle fd, int attr );
int fpui_get_window_attr( fpui_handle fd );
/*int fpui_get_attributes( fpui_handle fd, int index );*/
int fpui_set_character_blink( fpui_handle fd, bool state );
bool  fpui_get_character_blink( fpui_handle fd );
int fpui_set_backlight( fpui_handle fd, bool state );
bool  fpui_get_backlight( fpui_handle fd );
int fpui_set_backlight_timeout( fpui_handle fd, int timeout );
int fpui_set_cursor_blink( fpui_handle fd, bool state );
bool  fpui_get_cursor_blink( fpui_handle fd );
int fpui_set_reverse_video( fpui_handle fd, bool state );
bool  fpui_get_reverse_video( fpui_handle fd );
int fpui_set_underline( fpui_handle fd, bool state );
bool  fpui_get_underline( fpui_handle fd );
int fpui_set_auto_wrap( fpui_handle fd, bool state );
bool  fpui_get_auto_wrap( fpui_handle fd );
int fpui_set_auto_repeat( fpui_handle fd, bool state );
bool  fpui_get_auto_repeat( fpui_handle fd );
int fpui_set_cursor( fpui_handle fd, bool state );
bool  fpui_get_cursor( fpui_handle fd );
int fpui_set_auto_scroll( fpui_handle fd, bool state );
bool  fpui_get_auto_scroll( fpui_handle fd );
int fpui_reset_all_attributes( fpui_handle fd );
/*
	Read Operators
*/
int fpui_poll( fpui_handle fd, int flags );
ssize_t fpui_read( fpui_handle fd, char *buffer, int bufsize );
char fpui_read_char( fpui_handle fd );
ssize_t fpui_read_string( fpui_handle fd, char *string, int stringsize );
/*
	Write Operators
*/
ssize_t fpui_write( fpui_handle fd, char *buffer, int buflen );
ssize_t fpui_write_char( fpui_handle fd, char ch );
ssize_t fpui_write_string( fpui_handle fd, char *string );
ssize_t fpui_write_at( fpui_handle fd, char *buffer, int buflen, int row, int column );
ssize_t fpui_write_char_at( fpui_handle fd, char ch, int row, int column );
ssize_t fpui_write_string_at( fpui_handle fd, char *string, int row, int column );
/*
	Cursor Operators
*/
int fpui_get_cursor_pos( fpui_handle fd,  int *row, int *column );
int fpui_set_cursor_pos( fpui_handle fd,  int row, int column );
int fpui_home( fpui_handle fd );
int fpui_set_tab( fpui_handle fd );
int fpui_clear_tab( fpui_handle fd, int type );
/*
	Special Character Operators
*/
int fpui_compose_special_char( fpui_handle fd, int index, unsigned char *buffer );
int fpui_display_special_char( fpui_handle fd, int index );
/*
	LED Operators
*/
int fpui_set_led( fpui_handle fd, int state );
int fpui_get_led( fpui_handle fd );
/*
	AUX Switch Operators
*/
fpui_aux_handle fpui_open_aux_switch( void );
int fpui_close_aux_switch( fpui_aux_handle fd );
int fpui_read_aux_switch( fpui_aux_handle fd );
/*
	Key Mapping Operators
*/
typedef struct keymap_s {
	char	cmd;
	char	key;
	char	seq[7];
} keymap_t;

int fpui_set_keymap( fpui_handle fd, char key, char *seq );
int fpui_get_keymap( fpui_handle fd, char key, char *seq, int size );
int fpui_del_keymap( fpui_handle fd, char key );
int fpui_reset_keymap( fpui_handle fd, int type );



#endif
