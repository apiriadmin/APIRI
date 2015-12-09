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

#define _GNU_SOURCE
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>
#include <regex.h>
#include <errno.h>
#include <string.h>
#include <pthread.h>

#include <front_panel.h>


#undef DEBUG

#ifdef DEBUG
static FILE *log = NULL;
#define DBG(x...)           fprintf(log, x);
#else
#define DBG(x...)           do{}while(0)
#endif

#define CHAR_BS     0x08
#define CHAR_HT	    0x09
#define CHAR_LF	    0x0a
#define	CHAR_FF	    0x0c
#define	CHAR_CR	    0x0d
#define CHAR_DC1    0x11
#define CHAR_DC2    0x12
#define CHAR_DC3    0x13
#define CHAR_DC4    0x14
#define CHAR_ESCAPE 0x1b

#define TUI_MAJIC   0x5455495f

#define ABS(x) (x<0)?(-x):(x)

typedef enum { NORMAL, GROUP1, GROUP2 } state_t;
typedef enum { OFF, ON } state_v;

extern void viewport_copy_out( int, char* );
extern void tohex( char * );
extern int xprintf( int fd, const char * fmt, ... );
extern  void routing_return( int, char *, char *s );
extern int get_screen_type(char *);
extern int get_focus( void );
extern void set_focus( int );
extern bool panel_present( void );
extern int screen_XX;
extern int screen_YY;

void virtual_terminal_return( int , char* );


typedef struct display_s {			// this structure encapsulates the entire virtual display

	unsigned int	majic;			// majic number tu indicate this display structure is valid.
	pthread_mutex_t lock;
	pthread_cond_t  update;

	struct {				// this structure controls the cursor
		int	visible:1,		// the cursor is visible
			blink:1,		// the cursor is blinking
			underline:1;		// the cursor is underlined
		int	row;			// the current row the cursor is in
		int	column;			// the current column the cursor is in
	} cursor;

	struct {				// this structure controls the screen
		int	backlight:1,		// the backlight is illuminated
			reverse:1,		// the screen is in reverse video mode
			auto_wrap:1,		// Auto Wrap mode is enabled
			auto_repeat:1,		// Auto Repeat mode is enabled
			auto_scroll:1,		// Auto Scroll mode is enabled
			aux_switch:1;		// Last known AUX switch status 
		int	backlight_timeout;
		int	rows;			// the number of rows the display has available
		int	columns;		// the number of columns the display has available
	} screen;

	unsigned char	tab_stops[16];		// array of tab stop column positions
	unsigned char	special_chars[8][9];	// storage for up to 8 special characters (each cell is 8x8 pixels)

	int		underline_active;	// the last state of the underline attribute
	int		blink_active;		// the last state of the blink attribute
	int		reverse_active;		// the last state of the reverse video attribute
	int		emergency;

	struct onechar {			// this structure represents a single character
		char	underline:1,		// this character is underlined
			blink:1,		// this character is blinking
			reverse:1,		// this character is reverse video
			special:1;		// this character is bit mapped (c is the index)
		char	c;			// the ASCII character (or index of special character)
	} terminal[24][80];			// the working virtual display

	struct keycode_s {			
		char key;			// the returned character value (0 means no mapping)
		char code[7];			// the source character stream
	} keycode_map[16];			// allow up to 16 keys to be mapped.
} display_t;


display_t *display[FP_MAX_DEVS];		// array of pointers to virtual terminals

void cursor_position( display_t *, int, int[] );
void character( display_t *, int, int[] );
void screen( display_t *, int, int[] );
void other( display_t *, int, int[] );

#define ESC "\x1b"

typedef enum { TAB, RETURN, NEWLINE, BACKSPACE, ABSOLUTE, UP, DOWN, RIGHT, LEFT, HOME, SET_TAB, CLEAR_TAB,
		INQUIRE_POSITION } cursor_position_types;

typedef enum { COMPOSE_SPECIAL, DISPLAY_SPECIAL, BLINK_ON, BLINK_OFF, CURSOR_BLINK_ON, CURSOR_BLINK_OFF,
		REVERSE_VIDEO_ON, REVERSE_VIDEO_OFF, UNDERLINE_ON, UNDERLINE_OFF, ALL_ATTRIBUTES_OFF } character_types;

typedef enum { CLEAR, SOFT_RESET, BACKLIGHT_ON, BACKLIGHT_OFF, AUTO_WRAP_ON, AUTO_WRAP_OFF, AUTO_REPEAT_ON,
		AUTO_REPEAT_OFF, CURSOR_ON, CURSOR_OFF, AUTO_SCROLL_ON, AUTO_SCROLL_OFF, BACKLIGHT_TIMEOUT,
		INQUIRE_ATTRIBUTES } screen_types;

typedef enum { POWER_UP, INQUIRE_AUX, INQUIRE_HEATER, INQUIRE_TYPE, INQUIRE_FOCUS, PANEL_PRESENT } other_types;


// #define REGEX_INIT { NULL, 0, 0, 0, NULL, NULL, 0, 0, 0, 0, 0, 0, 0, 0 }
#define REGEX_INIT { .buffer = NULL }

/*
	NOTES:
		The sequence "[[]" in the string refers to the literal character '['
		The sequence "[?]" in the string refers to the literal character '?'
		The sequence "[0-9]+" in the string refers to one or more occurences of any character between '0' and '9', inclusive.
		The "^" anchors the pattern match to beginning of string.
*/

static struct command_table_s {
	char	*pattern;
	regex_t	preg;
	int	argc;
	void	(*func)( display_t *, int, int[] );
	int	type;
} cmd_tab[] = {
/*	{ "\t",				REGEX_INIT, 0, cursor_position,	TAB },
	{ "\r",				REGEX_INIT, 0, cursor_position,	RETURN },
	{ "\n",				REGEX_INIT, 0, cursor_position,	NEWLINE },
	{ "\b",				REGEX_INIT, 0, cursor_position,	BACKSPACE },*/
	{ "^" ESC "[[][0-9]+;[0-9]+f", 	REGEX_INIT, 2, cursor_position,	ABSOLUTE },
	{ "^" ESC "[[][0-9]+A",		REGEX_INIT, 1, cursor_position,	UP },
	{ "^" ESC "[[][0-9]+B",		REGEX_INIT, 1, cursor_position,	DOWN },
	{ "^" ESC "[[][0-9]+C",		REGEX_INIT, 1, cursor_position,	RIGHT },
	{ "^" ESC "[[][0-9]+D",		REGEX_INIT, 1, cursor_position,	LEFT },
	{ "^" ESC "[[]H",		REGEX_INIT, 0, cursor_position,	HOME },
	{ "^" ESC "[[]2J",		REGEX_INIT, 0, screen,		CLEAR },
	{ "^" ESC "c",			REGEX_INIT, 0, screen,		SOFT_RESET },
	{ "^" ESC "P[1-8][[]"/*([0-9]+[;])+[0-9]+f*/, REGEX_INIT, 7, character, COMPOSE_SPECIAL },
	{ "^" ESC "[[]<[0-9]+V",	REGEX_INIT, 1, character,	DISPLAY_SPECIAL },
	{ "^" ESC "[[]25h",		REGEX_INIT, 0, character,	BLINK_ON },
	{ "^" ESC "[[]25l",		REGEX_INIT, 0, character,	BLINK_OFF },
	{ "^" ESC "[[]<5h",		REGEX_INIT, 0, screen,		BACKLIGHT_ON },
	{ "^" ESC "[[]<5l",		REGEX_INIT, 0, screen,		BACKLIGHT_OFF },
	{ "^" ESC "[[]33h",		REGEX_INIT, 0, character,	CURSOR_BLINK_ON },
	{ "^" ESC "[[]33l",		REGEX_INIT, 0, character,	CURSOR_BLINK_OFF },
	{ "^" ESC "[[]27h",		REGEX_INIT, 0, character,	REVERSE_VIDEO_ON },
	{ "^" ESC "[[]27l",		REGEX_INIT, 0, character,	REVERSE_VIDEO_OFF },
	{ "^" ESC "[[]24h",		REGEX_INIT, 0, character,	UNDERLINE_ON },
	{ "^" ESC "[[]24l",		REGEX_INIT, 0, character,	UNDERLINE_OFF },
	{ "^" ESC "[[]0m",		REGEX_INIT, 0, character,	ALL_ATTRIBUTES_OFF },
	{ "^" ESC "H",			REGEX_INIT, 0, cursor_position,	SET_TAB },
	{ "^" ESC "[[][0-9]+g",		REGEX_INIT, 1, cursor_position,	CLEAR_TAB },
	{ "^" ESC "[[][?]7h",		REGEX_INIT, 0, screen,		AUTO_WRAP_ON },
	{ "^" ESC "[[][?]7l",		REGEX_INIT, 0, screen,		AUTO_WRAP_OFF },
	{ "^" ESC "[[][?]8h",		REGEX_INIT, 0, screen,		AUTO_REPEAT_ON },
	{ "^" ESC "[[][?]8l",		REGEX_INIT, 0, screen,		AUTO_REPEAT_OFF },
	{ "^" ESC "[[][?]25h",		REGEX_INIT, 0, screen,		CURSOR_ON },
	{ "^" ESC "[[][?]25l",		REGEX_INIT, 0, screen,		CURSOR_OFF },
	{ "^" ESC "[[]<47h",		REGEX_INIT, 0, screen,		AUTO_SCROLL_ON },
	{ "^" ESC "[[]<47l",		REGEX_INIT, 0, screen,		AUTO_SCROLL_OFF },
	{ "^" ESC "[[]<[0-9]+S",	REGEX_INIT, 1, screen,		BACKLIGHT_TIMEOUT },
	{ "^" ESC "[[]PU",		REGEX_INIT, 0, other,		POWER_UP },
	{ "^" ESC "[[]6n",		REGEX_INIT, 0, cursor_position,	INQUIRE_POSITION },
	{ "^" ESC "[[]Bn",		REGEX_INIT, 0, screen,		INQUIRE_ATTRIBUTES },
	{ "^" ESC "[[]An",		REGEX_INIT, 0, other,		INQUIRE_AUX },
	{ "^" ESC "[[]hn",		REGEX_INIT, 0, other,		INQUIRE_HEATER },
	{ "^" ESC "[[]c",		REGEX_INIT, 0, other,		INQUIRE_TYPE },
	{ "^" ESC "[[]Fn",		REGEX_INIT, 0, other,           INQUIRE_FOCUS },
	{ "^" ESC "[[]5n",		REGEX_INIT, 0, other,           PANEL_PRESENT },
};

#define CMD_TAB_SIZE ( sizeof( cmd_tab ) / sizeof( cmd_tab[0]) )

void clear_screen( display_t * );

static char return_buffer[128];
bool emergency_mode = false;

typedef struct {
	char	cmd;
	char	key;
	char	seq[8];
} keymap_t;

struct keycode_s default_keymap[] = {
	{ 0x80, { CHAR_ESCAPE, '[', 'A', '\0', '\0', '\0', '\0' } },
	{ 0x81, { CHAR_ESCAPE, '[', 'B', '\0', '\0', '\0', '\0' } },
	{ 0x82, { CHAR_ESCAPE, '[', 'C', '\0', '\0', '\0', '\0' } },
	{ 0x83, { CHAR_ESCAPE, '[', 'D', '\0', '\0', '\0', '\0' } },
	{ 0x84, { CHAR_ESCAPE, 'O', 'P', '\0', '\0', '\0', '\0' } },
	{ 0x85, { CHAR_ESCAPE, 'O', 'Q', '\0', '\0', '\0', '\0' } },
	{ 0x86, { CHAR_ESCAPE, 'O', 'R', '\0', '\0', '\0', '\0' } },
};

#define DEFAULT_KEYMAP_SIZE ( sizeof( default_keymap ) / sizeof( default_keymap[0] ) )

int keycomp( const void * a, const void * b )
{
	const struct keycode_s * akey = a;
	const struct keycode_s * bkey = b;

	// float all empty slots to the bottom of the list
	if( (akey->key == 0) && (bkey->key == 0) ) {
		return( 0 );
	} else if( akey->key == 0 ) {
		return( 1 );
	} else if( bkey->key == 0 ) {
		return( -1 );
	}

	return( strcmp( akey->code, bkey->code ) );
}


int is_active( int term )
{
	if( term < 0 ) 		  return( false );
	if( term >= FP_MAX_DEVS ) return( false );
	if( display[term] == NULL ) return( false );
	return( display[term]->majic == TUI_MAJIC );
}


void vt_lock( int term )
{
	pthread_mutex_lock( &display[term]->lock );
}


void vt_unlock( int term )
{
	pthread_mutex_unlock( &display[term]->lock );
}


void vt_wait_for_update( int term )
{
	pthread_mutex_lock( &display[term]->lock );
	pthread_cond_wait( &display[term]->update, &display[term]->lock );
	pthread_mutex_unlock( &display[term]->lock );
}


int create_virtual_terminal( int term )
{
	int i;

	printf("%s: term=%d\n", __func__, term );

#ifdef DEBUG
	if (log == NULL) {
		if( (log = fopen("/tmp/fpm_vt.log", "a+") ) == NULL ) {
			log = stderr;
		} 
	}
#endif

	if( (term < 0) || (term >= FP_MAX_DEVS) ) {
		return( -EINVAL );
	}
	if( display[term] != NULL ) {
		return( -EINVAL );
	}
	
	display[term] = calloc( 1, sizeof( display_t ) );
	if( display[term] == NULL ) {
		return( -ENOMEM );
	}

	pthread_mutex_init( &display[term]->lock, NULL );
	pthread_cond_init( &display[term]->update, NULL );
	// The following values as per Section 6.1.4.1 of ATC Standard Draft v6.08
	display[term]->screen.backlight_timeout = 6;
	display[term]->screen.rows    = /*16*/screen_YY;
	display[term]->screen.columns = /*40*/screen_XX;
	
	display[term]->tab_stops[0] = 9;
	display[term]->tab_stops[1] = 17;
	display[term]->tab_stops[2] = 25;
	display[term]->tab_stops[3] = 33;
	
	// set default stops every 8th column thereafter
	for( i = 4; (i * 8) <= display[term]->screen.columns; i++ ) {
		display[term]->tab_stops[i] = (i * 8) + 7;
	}

	// TODO need to clear key code mapping array
	memset( display[term]->keycode_map, 0, sizeof(display[term]->keycode_map) );

	// Load default ATC mappings
	for( i = 0; i < DEFAULT_KEYMAP_SIZE; i++ ) {
		display[term]->keycode_map[i] = default_keymap[i];
	}

	display[term]->majic = TUI_MAJIC;

	clear_screen( display[term] );

	return( 0 );
}

void destroy_virtual_terminal( int term )
{
	printf("%s: term=%d\n", __func__, term );

	if( ! is_active( term ) ) {
		return;
	}

	if (get_focus() == term) {
		// destroyed window has focus, so revert to MS_DEV
		set_focus( MS_DEV );
	}
	pthread_mutex_destroy( &display[term]->lock );
	pthread_cond_destroy( &display[term]->update );
	memset( display[term], 0, sizeof( display_t ) );
	free( display[term] );
	display[term] = NULL;
}

void refresh_virtual_terminal(int term)
{
	printf("%s: term=%d\n", __func__, term );
	if (get_focus() == term) {
		// redraw virtual display to panel
		set_focus(term);
	}
}

int getterm( display_t * disp )
{
	int i;
	for( i = 0; i < FP_MAX_DEVS; i++ ) {
		if( display[i] == disp ) {
			return( i );
		}
	}
	return( -1 );
}

void set_emergency(int term, int state)
{
	int i;
	
	if (get_focus() == term)
		state = 0;
		
	display[term]->emergency = state;
	// set global emergency condition
	for (i=0; i<FP_MAX_DEVS; i++) {
		if ((display[i] != NULL) && (display[i]->majic == TUI_MAJIC)) {
			if (display[i]->emergency) {
				emergency_mode = true;				
				return;
			}
		}
	}
	emergency_mode = false;
}

//
// Cursor control routines
//
// These routines get or set the position of the cursor
// independent of the auto_wrap and auto_scroll attributes
//

// this routine returns the location of the cursor in the
// two integer pointers passed in.
void get_cursor( display_t * disp, int * row, int * column )
{
	*row    = disp->cursor.row;
	*column = disp->cursor.column;
}


// this routine sets the cursors position to the row and column passed
// in, if the final position remains within the active screen.
void set_cursor( display_t * disp, int row, int column )
{
	if( (row >= 0) && (row < disp->screen.rows) ) {
		disp->cursor.row = row;
	}

	if( (column >= 0) && (column < disp->screen.columns) ) {
		disp->cursor.column = column;
	}
}


//
// this helper routine returns the character 'h' or 'l' based
// on the boolean state of the passed arguement. This is used
// to create return strings from inquiry requests.
//
char hl( int state )
{
	return( (state)?'h':'l' );
}


#if 0	// dead code ???
// this routine offsets the cursor from it's current position
// negative values move the cursor left or up while positive
// values move it right or down.
void set_cursor_delta( display_t * disp, int row, int column )
{
	row    += disp->cursor.row;
	column += disp->cursor.column;
	set_cursor( disp, row, column );
}
#endif


//
// this routine returns the column of the first tab stop
// past the cursors current location.
//
int find_next_tab( display_t * disp )
{
	int i;

	for( i = 0; i < 16; i++ )
	    {
	    	if( disp->tab_stops[i] >= disp->cursor.column )
		    return( disp->tab_stops[i] );
	    }
	return( 0 );
}


//
// This is a helper routine to move some number of
// lines from source to destination, care is taken to
// ensure proper behavior if source and destination
// overlap. Each line is assumed to be the length
// specified by the display.screen.columns setting.
//
void move_lines( display_t * disp, int src, int dest, int lines )
{
	int len = disp->screen.columns * sizeof( disp->terminal[0][0] );
	memmove( &disp->terminal[dest][0], &disp->terminal[src][0], len * lines );
}


//
// This helper routine simple clears the specified line
// in the specified display terminal. It sets the
// character value to ' ' and clears all character attributes.
//
void clear_line( display_t *disp, int line )
{
	int i;
	for(i=0; i<disp->screen.columns; i++) {
		disp->terminal[line][i].c = ' ';
                disp->terminal[line][i].underline = 0;
                disp->terminal[line][i].blink = 0;
                disp->terminal[line][i].reverse = 0;
                disp->terminal[line][i].special = 0;
	}
}


//
// this routine scrolls the screen some number of lines
// up or down, depending on the count passed in. To
// scroll up one line, 7 lines beginning with line 1
// (not 0) are moved to line 0. Line 7 is then erased.
// To scroll up 2 lines, 6 lines beginning with line 2
// are moved to line 0. Lines 6 and 7 are then erased.
// To scroll down one line, 7 lines beginning with line 0
// are moved to line 1. Line 0 is then erased.
//
int scroll_screen( display_t * disp, int cnt )
{
	int i;
	// if cnt is zero, don't do anything except return 0
	if( cnt > 0 )		// scroll screen up
	    {
		move_lines( disp, cnt, 0, disp->screen.rows - cnt );
		for( i = disp->screen.rows - cnt; i < disp->screen.rows; i++ )
			clear_line( disp, i );

	    }
	else if( cnt < 0 )	// scroll screen down
	    {
	    	cnt = ABS( cnt );
		move_lines( disp, 0, cnt, disp->screen.rows - cnt );
		for( i = 0; i < cnt; i++ )
			clear_line( disp, i );
	    }
	return( cnt );
}


//
// This helper routine clears the entire screen using
// the clear_line method above and the number of rows
// from the specified display.
//
void clear_screen( display_t * disp )
{
	int	row;

	for( row = 0; row < disp->screen.rows; row++ )
		clear_line( disp, row );
}


//
// This helper routine performs a soft reset
// as per ATC5201 Standard section 6.1.4.1
//
void reset_screen( display_t * disp )
{
        int i = 0;
        
        disp->screen.auto_wrap = 0;
        disp->screen.auto_scroll = 0;
        disp->tab_stops[0] = 9;
        disp->tab_stops[1] = 17;
        disp->tab_stops[2] = 25;
        disp->tab_stops[3] = 33;
        // set default stops every 8th column thereafter
        for( i = 4; (i * 8) <= disp->screen.columns; i++ ) {
                disp->tab_stops[i] = (i * 8) + 7;
        }
        disp->screen.backlight_timeout = 6;
        disp->screen.backlight = 0;
        clear_screen( disp );
}


void cursor_position( display_t * disp, int type, int args[] )
{
#ifdef DEBUG
	int term = getterm( disp );
#endif
	char buf[32];
	int row;
	int column;

	switch( type ) {
		case TAB:
			disp->cursor.column = find_next_tab( disp );		// move the cursor to the next tab stop
			break;
		case RETURN:
			disp->cursor.column = 0;				// move the cursor to the beginning of the current line
			break;
		case NEWLINE:
			disp->cursor.row++;					// move the cursor to the next line
			if( disp->cursor.row >= disp->screen.rows ) {		// if the cursor moved past the end of the screen ...
				if( disp->screen.auto_scroll ) {		//   and auto_scroll is enabled
					scroll_screen( disp, 1 );		// scroll the screen up one line
					disp->cursor.row--;			// correct the cursor row position
				} else {					//   and if auto_scroll is not enabled
					disp->cursor.row = 0;			// move the cursor to the first line
				}
			}
			break;
		case BACKSPACE:
			if( disp->cursor.column > 0 ) {				// if cursor is not at the beginning of the line
				disp->cursor.column--;				// just back it up one space
			} else if( disp->screen.auto_wrap ) {			// otherwise, if auto_wrap is enabled
				disp->cursor.column = disp->screen.columns - 1;	// move the cursor to the end of the line
				if( disp->cursor.row == 0 ) {			//   and the cursor is in the first row
					disp->cursor.row = disp->screen.rows - 1;	// move the cursor to the last line
				} else {					// the cursor is not in the first row
					disp->cursor.row--;			// move the cursor up one line
				}
			}
			break;
		case ABSOLUTE:
			DBG( "%s(%d): HVP (%d,%d)\n", __func__, term, args[0], args[1] );
			set_cursor( disp,  args[0]-1, args[1]-1 );
			break;
		case UP:
			DBG( "%s(%d): CUP (%d)\n", __func__, term, args[0] );
			get_cursor( disp, &row, &column );
			set_cursor( disp, row-args[0], column );
			break;
		case DOWN:
			DBG( "%s(%d): CUD (%d)\n", __func__, term, args[0] );
			get_cursor( disp, &row, &column );
			set_cursor( disp, row+args[0], column );
			break;
		case RIGHT:
			DBG( "%s(%d): CUF (%d)\n", __func__, term, args[0] );
			get_cursor( disp, &row, &column );
			set_cursor( disp, row, column+args[0] );
			break;
		case LEFT:
			DBG( "%s(%d): CUB (%d)\n", __func__, term, args[0] );
			get_cursor( disp, &row, &column );
			set_cursor( disp, row, column-args[0] );
			break;
		case HOME:
			DBG( "%s(%d): CUH \n", __func__, term );
			set_cursor( disp, 0, 0 );
			break;
		case SET_TAB:
			DBG( "%s(%d): SHT \n", __func__, term );
			get_cursor( disp, &row, &column );
			break;
		case CLEAR_TAB:
			DBG( "%s(%d): CHT \n", __func__, term );
			break;
		case INQUIRE_POSITION:
			get_cursor( disp, &row, &column );
			sprintf( buf, "\x1b[%d;%dR", row+1, column+1 );
			routing_return( getterm( disp ), buf, NULL );
			break;
	}
}


void character( display_t *disp, int type, int args[] )
{
#ifdef DEBUG
	int term = getterm( disp );
#endif
	int i;
	int row;
	int column;

	switch( type ) {
		case COMPOSE_SPECIAL:
			DBG( "%s(%d): SPC @ %d\n", __func__, term, args[0] );
			for( i = 1; i < 7/*8*/; i++ ) {
				disp->special_chars[args[0]][i] = args[i];
			}
			disp->special_chars[args[0]][0] = 1; //indicate configured
			break;
		case DISPLAY_SPECIAL:
			DBG( "%s(%d): SPD @ %d\n", __func__, term, args[0]);
	    		disp->terminal[ disp->cursor.row ][ disp->cursor.column ].c = args[0];
	    		disp->terminal[ disp->cursor.row ][ disp->cursor.column ].special = 1;
			get_cursor( disp, &row, &column );
			set_cursor( disp, row, column+1 );	    		
			break;
		case BLINK_ON:
			DBG( "%s(%d): CHR-BK ON \n", __func__, term );
			disp->blink_active = 1;
			break;
		case BLINK_OFF:
			DBG( "%s(%d): CHR-BK OFF \n", __func__, term );
			disp->blink_active = 0;
			break;
		case CURSOR_BLINK_ON:
			DBG( "%s(%d): CUR-BK ON \n", __func__, term );
			disp->cursor.blink = 1;
			break;
		case CURSOR_BLINK_OFF:
			DBG( "%s(%d): CUR-BK OFF \n", __func__, term );
			disp->cursor.blink = 0;
			break;
		case REVERSE_VIDEO_ON:
			DBG( "%s(%d): CHR-RV ON \n", __func__, term );
			disp->reverse_active = 1;
			break;
		case REVERSE_VIDEO_OFF:
			DBG( "%s(%d): CHR-RV OFF \n", __func__, term );
			disp->reverse_active = 0;
			break;
		case UNDERLINE_ON:
			DBG( "%s(%d): CHR-UL ON \n", __func__, term );
			disp->underline_active = 1;
			break;
		case UNDERLINE_OFF:
			DBG( "%s(%d): CHR-UL OFF \n", __func__, term );
			disp->underline_active = 0;
			break;
		case ALL_ATTRIBUTES_OFF:
			DBG( "%s(%d): CHR-ALL OFF \n", __func__, term );
			break;
	}
}


void screen( display_t * disp, int type, int args[] )
{
#ifdef DEBUG
	int term = getterm( disp );
#endif
	char buf[32];
	int row;
	int column;

	switch( type ) {
		case CLEAR:
			DBG( "%s(%d): SRC \n", __func__, term );
			get_cursor( disp, &row, &column );
			clear_screen( disp );
			set_cursor( disp, row, column );
			break;
		case SOFT_RESET:
			DBG( "%s(%d): SRS \n", __func__, term );
                        reset_screen( disp);
			break;
		case BACKLIGHT_ON:
			DBG( "%s(%d): SCR-BL ON \n", __func__, term );
			disp->screen.backlight = 1;
			break;
		case BACKLIGHT_OFF:
			DBG( "%s(%d): SCR-BL OFF \n", __func__, term );
			disp->screen.backlight = 0;
			break;
		case AUTO_WRAP_ON:
			DBG( "%s(%d): SCR-AW ON \n", __func__, term );
			disp->screen.auto_wrap = 1;
			break;
		case AUTO_WRAP_OFF:
			DBG( "%s(%d): SCR-AW OFF \n", __func__, term );
			disp->screen.auto_wrap = 0;
			break;
		case AUTO_REPEAT_ON:
			DBG( "%s(%d): SCR-AR ON \n", __func__, term );
			disp->screen.auto_repeat = 1;
			break;
		case AUTO_REPEAT_OFF:
			DBG( "%s(%d): SCR-AR OFF \n", __func__, term );
			disp->screen.auto_repeat = 0;
			break;
		case CURSOR_ON:
			DBG( "%s(%d): CUR-VS ON \n", __func__, term );
			disp->cursor.visible = 1;
			break;
		case CURSOR_OFF:
			DBG( "%s(%d): CUR-VS OFF \n", __func__, term );
			disp->cursor.visible = 0;
			break;
		case AUTO_SCROLL_ON:
			DBG( "%s(%d): SCR-AS ON \n", __func__, term );
			disp->screen.auto_scroll = 1;
			break;
		case AUTO_SCROLL_OFF:
			DBG( "%s(%d): SCR-AS OFF \n", __func__, term );
			disp->screen.auto_scroll = 0;
			break;
		case BACKLIGHT_TIMEOUT:
			DBG( "%s(%d): SBTO  (%d)\n", __func__, term, args[0] );
			disp->screen.backlight_timeout = args[0];
			break;
		case INQUIRE_ATTRIBUTES:
			DBG( "%s(%d): ATT \n", __func__, term );
			sprintf( buf, "\x1b[%c;%c;%c;%c;%d;%c;%c;%c;%c;%c;%cR",
				hl( disp->screen.auto_wrap ),
				hl( disp->screen.auto_scroll ),
				hl( disp->screen.auto_repeat ),
				hl( disp->screen.backlight ),
				disp->screen.backlight_timeout,
				hl(disp->screen.aux_switch),
				hl(disp->cursor.visible),
				hl(disp->cursor.blink),
				hl(disp->terminal[disp->cursor.row][disp->cursor.column].blink),
				hl(disp->terminal[disp->cursor.row][disp->cursor.column].reverse),
				hl(disp->terminal[disp->cursor.row][disp->cursor.column].underline));
			routing_return( getterm( disp ), buf, NULL );
			break;
	}
}


void other( display_t *disp, int type, int args[] )
{
	int term = getterm( disp );
	char buf[32];

	switch( type ) {
		case POWER_UP:
			break;
		case INQUIRE_AUX:
			DBG( "%s(%d): AUX \n", __func__, term );
			if (get_focus() == term) {
				sprintf( buf, "\x1b[An");
				viewport_copy_out( term, buf );
			} else {
				sprintf( buf, "\x1b[?R");
				routing_return( term, buf, NULL );
			}
			break;
		case INQUIRE_HEATER:
			DBG( "%s(%d): HEATER \n", __func__, term );
			break;
		case INQUIRE_TYPE: {
			char screen_type = 'B';
			get_screen_type(&screen_type);
			DBG( "%s(%d): TYPE %c\n", __func__, term, screen_type );
			sprintf( buf, "\x1b[%cR", screen_type );
			routing_return( term, buf, NULL );
			break;
                case INQUIRE_FOCUS: {
			DBG( "%s(%d): [FOCUS] \n", __func__, term );
                        sprintf( buf, "\x1b[%cR", (get_focus()==term)?'h':'l');
                        routing_return( term, buf, NULL );
                        }
                        break;
                case PANEL_PRESENT: {
			DBG( "%s(%d): [PRESENCE] \n", __func__, term );
                        sprintf( buf, "\x1b[%cn", panel_present()?'0':'3');
                        routing_return( term, buf, NULL );
                        }
                        break;
		}
	}
}





//
// this routine helps decode escape sequences by reading out
// and number arguements and loading them into a list.
//	s = pointer to first arguement in string
//	args = pointer to array of integers to return arguements in
//	argc = the maximum number of items args can hold
//		loaded into the array.
//
//	return the number of items loaded into args
//
// NOTE: Some commands have numbers as part of their command code.
//	 These will be decoded and loaded into the array. The consuming
//	 application must account for these entries.
//
int parse_arg_list( char *s, int args[], int argc )
{
	int i;
	int k = 0;

	args[0] = 0;

	for( i = 0, k = 0; (k < argc) && s[i]; i++ ) {
		if( isdigit( s[i] ) ) {
			args[k] = 0;
			while( isdigit( s[i] ) ) {
				args[k] = (args[k] * 10) + s[i++] - '0';
			}
			DBG( "%s: adding arg[%d] = %d\n", __func__, k, args[k] );
			k++;
		}
	}
	return( k );
}


//
// this routine parses and executes the escape sequence pointed to
// by 's'. Undefined or unsupported sequences are simply skipped
// over. The number of characters parsed out of the string is returned.
//
int parse_escape_request( display_t *disp, char *s )
{
	int i;
	int errcode;
	regmatch_t pmatch[2];
	char errbuf[128];
	int args[16];

	DBG( "%s: parsing %d bytes = <ESC>%s\n", __func__, strlen(s), &s[1] );

	// make sure the command table is initialized.
	// TODO this should be moved to an init routine
	if( cmd_tab[0].preg.buffer == NULL ) {
		for( i = 0; i < CMD_TAB_SIZE; i++ ) {
			if( (errcode = regcomp( &cmd_tab[i].preg, cmd_tab[i].pattern, REG_EXTENDED )) != REG_NOERROR ) {
				regerror( errcode, &cmd_tab[i].preg, errbuf, sizeof( errbuf ));
				DBG( "%s: regcomp failed at cmd_tab[%d] (%s)\n", __func__, i, errbuf );
			}
		}
	}

	for( i = 0; i < CMD_TAB_SIZE; i++ ) {
		if( (errcode = regexec( &cmd_tab[i].preg, s, 2, pmatch, 0)) == REG_NOERROR ) {
			// pmatch[0].rm_so == byte offset of first character in 's' that matches pattern.
			// pmatch[0].rm_eo == byte offset of last character in 's' that matches pattern.
			DBG( "%s: match on cmd_tab[%d] @%d for %d = <ESC>%s\n", __func__, i, pmatch[0].rm_so, pmatch[0].rm_eo - pmatch[0].rm_so, &cmd_tab[i].pattern[2] );
			// memmove( s[pmatch[0].rm_so], s[pmatch[0].rm_eo+1], pmatch[0].rm_eo - pmatch[0].rm_so );

			parse_arg_list( s, args, cmd_tab[i].argc ); // decode any numbers that are part of the command

			cmd_tab[i].func( disp, cmd_tab[i].type, args );	// call the operational routine for this command

			return( pmatch[0].rm_eo - pmatch[0].rm_so );	// return the number of characters consumed by this command
		} else if( errcode != REG_NOMATCH ) {
			regerror( errcode, &cmd_tab[i].preg, errbuf, sizeof( errbuf ));
			DBG( "%s: regexec failed at cmd_tab[%d] (%s)\n", __func__, i, errbuf );
		}
	}
	
	// If we can't parse the escape return 2 bytes as consumed so we continue past the calling loop
	return 2;
}



//
// This is the main controlling routine for the virtual terminal.
//
// When VirtualTerminalControl receives a DATA packet from the
// Front Panel Driver, it extracts the target virtual terminal
// and the data string and passes them here. Strings are expected
// to be standard NULL terminated character arrays. If the data
// string contains an inquiry escape sequence, the parse_escape_request
// routine will handle it (and all other escape sequences) and send a
// new data response packet back to the originating process.
//
void virtual_terminal( int terminal, char *s )
{
	int i;
	int j;
	display_t *disp = display[terminal];
	keymap_t  *km;


	if( disp == NULL ) {
		printf("%s: term %d No display device exists!\n", __func__, terminal );
		return;
	}

	DBG( "%s: term %d (%d,%d)\n", __func__, terminal, disp->screen.rows, disp->screen.columns );

	pthread_mutex_lock( &disp->lock );		// lock this terminal while updating it

	for( i = 0; s[i]; i++ ) {
		// handle any escape sequences 
	    	if( s[i] == CHAR_ESCAPE ) {
			i += parse_escape_request( disp, &s[i] ) - 1;
			continue;
		}

		// the rest should be normal characters
		switch( s[i] ) {
			case CHAR_CR:	// Carriage Return
				DBG( "%s(%d)   [0x%2.2x] @ (%d,%d)\n", __func__, terminal, s[i], disp->cursor.row, disp->cursor.column );
				disp->cursor.column = 0;			// move the cursor to the beginning of the current line
				break;
			case CHAR_LF:	// Line Feed
				DBG( "%s(%d)   [0x%2.2x] @ (%d,%d)\n", __func__, terminal, s[i], disp->cursor.row, disp->cursor.column );
				disp->cursor.row++;				// move the cursor to the next line
				if( disp->cursor.row >= disp->screen.rows ) {	// if the cursor moved past the end of the screen ...
					if( disp->screen.auto_scroll ) {	//   and auto_scroll is enabled
						scroll_screen( disp, 1 );	// scroll the screen up one line
						disp->cursor.row--;		// correct the cursor row position
					} else {				//   and if auto_scroll is not enabled
						disp->cursor.row = 0;		// move the cursor to the first line
					}
				}
				// Automatically add CR for Linux newline
				disp->cursor.column = 0;
				break;
			case CHAR_BS:	// Back Space
				if( disp->cursor.column > 0 ) {			// if cursor is not at the beginning of the line
					disp->cursor.column--;			// just back it up one space
				} else if( disp->screen.auto_wrap ) {		// otherwise, if auto_wrap is enabled
					disp->cursor.column = disp->screen.columns - 1;	// move the cursor to the end of the line
					if( disp->cursor.row == 0 ) {		//   and the cursor is in the first row
						disp->cursor.row = disp->screen.rows - 1;	// move the cursor to the last line
					} else {				// the cursor is not in the first row
						disp->cursor.row--;		// move the cursor up one line
					}
				}
				
				break;
			case CHAR_FF:	// Form Feed
				clear_screen( disp );				// clear the entire screen
				disp->cursor.row    = 0;			// move the cursor to the first row
				disp->cursor.column = 0;			// move the cursor to the first column
				break;
			case CHAR_HT:	// Horizontal Tab
				disp->cursor.column = find_next_tab( disp );	// move the cursor to the next tab stop
				break;
			case CHAR_DC1:	// add a keycode mapping
				DBG( "%s(%d)   [KeyMapSet]\n", __func__, terminal);
				km = (keymap_t *)&s[i];
				for( j = 0; j < 16; j++) {
					if( disp->keycode_map[j].key == '\0' ) {
						memmove( disp->keycode_map[j].code, km->seq, 7);
						disp->keycode_map[j].key = km->key;
						break;
					}
				}
				i += 8;	// skip over the 8 bytes that make up  this operation
				qsort( disp->keycode_map, 16, sizeof( struct keycode_s ), keycomp );
				goto clean_up;
				break;
			case CHAR_DC2:	// read back a keycode mapping
				DBG( "%s(%d)   [KeyMapGet]\n", __func__, terminal);
				km = (keymap_t *)&s[i];
				for( j = 0; j < 16; j++) {
					if( disp->keycode_map[j].key == km->key ) {
						km = (keymap_t *)return_buffer;
						km->cmd = CHAR_DC2;
						km->key = disp->keycode_map[j].key;
						memmove( km->seq, disp->keycode_map[j].code, 7 );
						virtual_terminal_return( terminal, (char *)km );
						break;
					}
				}
				i += 1;
				goto clean_up;
				break;
			case CHAR_DC3:	// delete a keycode mapping
				DBG( "%s(%d)   [KeyMapDel]\n", __func__, terminal);
				km = (keymap_t *)&s[i];
				for( j = 0; j < 16; j++) {
					if( disp->keycode_map[j].key == km->key ) {
						memset( disp->keycode_map[j].code, 0, 8 );
						break;
					}
				}
				i += 1;	// skip over the key byte used in this operation
				qsort( disp->keycode_map, 16, sizeof( struct keycode_s ), keycomp );
				goto clean_up;
				break;
			case CHAR_DC4:	// reset the entire keycode mapping table
				DBG( "%s(%d)   [KeyMapReset]\n", __func__, terminal);
				km = (keymap_t *)&s[i];
				if( km->key == '0' ) {	// clear the entire map
					for( j = 0; j < 16; j++) {
						disp->keycode_map[j].key = 0;
					}
				} else if( km->key == '1' ) {	// restore the default values.
					for( j = 0; j < DEFAULT_KEYMAP_SIZE; j++ ) {
						disp->keycode_map[j] = default_keymap[j];
					}
				}
				i += 1;
				goto clean_up;
				break;
			case '\0':	// End of string??
				// should never get here, but the line if done.
				break;
			default:
				// skip any non-printable (control) characters we don't support
				if( ! isprint( s[i] ) ) continue;

				DBG( "%s(%d) %c [0x%2.2x] @ (%d,%d)\n", __func__, terminal, s[i], s[i], disp->cursor.row, disp->cursor.column );
				// write the character at the current location of the cursor
	    			disp->terminal[ disp->cursor.row ][ disp->cursor.column ].c         = s[i];
	    			disp->terminal[ disp->cursor.row ][ disp->cursor.column ].underline = disp->underline_active;
	    			disp->terminal[ disp->cursor.row ][ disp->cursor.column ].blink     = disp->blink_active;
	    			disp->terminal[ disp->cursor.row ][ disp->cursor.column ].reverse   = disp->reverse_active;
	    			disp->terminal[ disp->cursor.row ][ disp->cursor.column ].special   = 0;

				// advance the cursor, we'll handle scroll and wrap modes next.
				disp->cursor.column++;

				// NOTE:
				// wrapping effects what happens at the end of a line.
				// wrapping on: characters past EOL begin on the next line, cursor advances
				// wrapping off: characters past EOL overwrite EOL, cursor stays at last column
				//
				// scrolling effects what happens at the bottom of the display.
				// scrolling on: a NEWLINE from the last line causes display to scroll up
				//	one line and the cursor to move to the beginning of last line.
				// scrolling off: a NEWLINE from the last line causes the cursor to move
				//	to the beginning of the top line

				// if the cursor is past the end of the line ...
				if( disp->cursor.column >= disp->screen.columns )
				    {
					if( disp->screen.auto_wrap )
					    {	// auto wrap is enabled, cursor goes to beginning of NEXT line
					    	disp->cursor.column = 0;
					    	disp->cursor.row++;
					    }
					else
					    {	// no auto wrap, cursor stays at the end of the line
					    	disp->cursor.column--;
					    }
				    }

				// if the cursor is past the bottom of the display
				if( disp->cursor.row >= disp->screen.rows )
				    {
					if( disp->screen.auto_scroll )
					    {	// auto scroll enabled, screen scrolls, cursor to beginning of last line
						scroll_screen( disp, 1 );	// scroll screen up 1 line
					    	disp->cursor.row--;		// keep cursor on botton line
					    }
					else 
					    {	// no auto scroll, cursor goes to top of screen
					    	disp->cursor.row    = 0;	// move cursor to top line
					    }
				    }
				break;
		    }
	    }

	viewport_copy_out( terminal, s );

clean_up:
	pthread_cond_signal( &disp->update );		// signal that the display has updated.
	pthread_mutex_unlock( &disp->lock );		// lock this terminal while updateing it
}


void load_screen( int fd, int term )
{
	display_t *disp = display[term];
	int char_underline = 0;
	int char_blink     = 0;
	int char_reverse   = 0;
	int r, c;
	int i, j;
	//int fd = fileno( fp );
	
	if (!is_active( term ))
		return;

	DBG("%s: fd=%d, term=%d (%d:%d)\n", __func__, fd, term, disp->screen.rows, disp->screen.columns );

	// clear screen
	xprintf( fd, ESC "[2J" );

	// home the cursor
	xprintf( fd, ESC "[H" );

	// disable screen attributes (to prevent extra wrap/scrolling etc)
	xprintf( fd, ESC "[?7l" );
	xprintf( fd, ESC "[?8l" );
	xprintf( fd, ESC "[<47l" );
	
	// set default character attributes, these will get
	// updated as characters are sent out.
	xprintf( fd, ESC "[24l" );	// Turn underline off
	xprintf( fd, ESC "[25l" );	// Turn blink off
	xprintf( fd, ESC "[27l" );	// Turn reverse video off


	DBG("%s: attributes set, loading text\n", __func__ );
	
	// send the special character definitions, if configured
	for( i = 0; i < 8; i++ ) {
		if (disp->special_chars[i][0] != 0) {
			xprintf( fd, ESC "P%d[%d", i+1, disp->special_chars[i][0] );
			for( j = 1; j < 8; j++ ) {
				xprintf( fd, ";%d", disp->special_chars[i][j] );
			}
			xprintf( fd, "f" );
		}
	}

	for( r = 0; r < disp->screen.rows; r++ ) {
		DBG( "%s(%d): @ (%d,%d)\n", __func__, term, r, 0 );
		xprintf( fd, ESC "[%d;%df", r+1, 1 );	// set the cursor position to beginning of this row
		for( c = 0; c < disp->screen.columns; c++ ) {
			// check the current character attributes against the current character
			if( char_underline != disp->terminal[r][c].underline ) {
				xprintf( fd, ESC "[24%c", hl( disp->terminal[r][c].underline ) );
				char_underline = disp->terminal[r][c].underline;
			}

			if( char_blink != disp->terminal[r][c].blink ) {
				xprintf( fd, ESC "[25%c", hl( disp->terminal[r][c].blink ) );
				char_blink = disp->terminal[r][c].blink;
			}

			if( char_reverse != disp->terminal[r][c].reverse ) {
				xprintf( fd, ESC "[27%c", hl( disp->terminal[r][c].reverse ) );
				char_reverse = disp->terminal[r][c].reverse;
			}
			
			if( disp->terminal[r][c].special ) {
				xprintf( fd, ESC "[<%dV", disp->terminal[r][c].c );
			} else {
				DBG( "%s(%d): %c [0x%2.2x] @ (%d,%d) ul=%d bl=%d rv=%d\n", __func__, term,
					disp->terminal[r][c].c, disp->terminal[r][c].c, r, c,
					char_underline, char_blink, char_reverse );

				if( isprint( disp->terminal[r][c].c ) ) {
					xprintf(fd, "%c", disp->terminal[r][c].c );
				} else {
					// if unprintable, just move cursor along
					xprintf(fd, ESC "[1C");
				}
			}
		}
		//fsync( fd );
	}

	// restore screen attributes
	xprintf( fd, ESC "[?7%c",  hl( disp->screen.auto_wrap ) );
	xprintf( fd, ESC "[?8%c",  hl( disp->screen.auto_repeat ) );
	xprintf( fd, ESC "[<47%c", hl( disp->screen.auto_scroll ) );

	// set cursor attributes and position
	xprintf( fd, ESC "[?25%c", hl( disp->cursor.visible ) );
	xprintf( fd, ESC "[33%c",  hl( disp->cursor.blink ) );
	// xprintf( fd, "", hl( disp->cursor.underline ) );
	xprintf( fd, ESC "[%d;%df",  disp->cursor.row+1, disp->cursor.column+1 );

	set_emergency(term, 0);
	
	DBG("%s: Text loaded\n", __func__ );
}



void virtual_terminal_return( int term, char *s )
{
	char   raw[64];
	char *t;
	int    i;

	if( !is_active( term ) ) {
		fprintf(stderr, "%s: term not active!\n", __func__);
		return;
	}

	// 3.3.1.2-x
	// search for escape sequences in the returned string and map
	// them to a single character as defined by the keycode_map for
	// this terminal. strings without any mappings should be passed 
	// through unchanged.
	// in order to support both mapped and direct modes, 2 replies need
	// to be returned to the driver, the direct, raw response and the
	// mapped response. These MUST be paired.

	strcpy( raw, s );
	DBG("%s:(%d)(%d), slen=%d %02x %02x %02x %02x %02x %02x %02x %02x\n",
		__func__, term, get_focus(), strlen(s),
		raw[0], raw[1], raw[2], raw[3], raw[4], raw[5], raw[6], raw[7]  );

	for( i = 0; (i < 16) && (display[term]->keycode_map[i].key != 0); i++ ) {
		if( (t = strstr( s, display[term]->keycode_map[i].code)) != NULL ) {
			int len = strlen( display[term]->keycode_map[i].code );	// get the size of the escape sequence
			*t = display[term]->keycode_map[i].key;			// copy the key to the buffer
			strcpy( t+1, t+len );					// shift string left over remainder of sequence
			i = -1;							// restart search in case of multiple hits.
		}
	}
	//DBG("%s: call routing_return()\n", __func__ );

	routing_return( term, s, raw );
}



void dump_virtual_terminals()
{
	display_t * disp;
	int i;
	int r;
	int c;

	for( i = 0; i < FP_MAX_DEVS; i++ ) {

		DBG( "VT %d: ", i );

		if( display[i] == NULL ) {
			DBG( "not in use.\n" );
			continue;
		}

		disp = display[i];

		DBG( "%d rows, %d columns\n", disp->screen.rows, disp->screen.columns );
		for( r = 0; r < disp->screen.rows; r++ ) {
			DBG( "r%d |", r );
			for( c = 0; c < disp->screen.columns; c++ ) {
				if( isprint( disp->terminal[r][c].c ) ) {
					DBG( "%c", disp->terminal[r][c].c );
				} else {
					DBG( "." );
				}
				// fprintf( stderr, "0x%2.2x ", disp->terminal[r][c].c );
			}
			DBG( "|\n" );
		}
	}
}
