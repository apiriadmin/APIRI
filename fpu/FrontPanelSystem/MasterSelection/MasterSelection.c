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
#include <ctype.h>

#include <fpui.h>
#include <front_panel.h>

#define CHAR_ESC '\x1b'

static int msi;				// globaled for signal handler
static int default_screen = MS_DEV;	// temporary storage for default screen

extern void tohex( char * );
extern void tohexn( char *, int );
extern int xprintf( int fd, const char * fmt, ... );

void window_handler( int arg )
{
	fprintf( stderr, "SIGWINCH\n" );
}


void alldone( int arg )
{
	fprintf( stderr, "Exiting on HUP\n");
	close( msi );
	exit( 0 );
}


static char *regtab[APP_OPENS];
#define MAX_MENU_ROWS 8
static char menu[MAX_MENU_ROWS][41];
int g_rows = 8, g_cols = 40;
static char pad[16];

int xprintat( int fd, int r, int c, const char *fmt, ... )
{
	int       n;
	va_list   ap;

	// set the cursors position
	n = xprintf( fd, "%c[%d;%df", CHAR_ESC, r, c );
	if( n > 0 ) {
		// issue a normal write
		va_start( ap, fmt );
		n = xprintf( fd, fmt, ap );
		va_end( ap );
	}

	return( n );
}


void clear_menu( void )
{
	memset( &menu[0][0], ' ', sizeof(menu) );
}


char *reg_name( int n )
{
	if( regtab[n] != NULL ) {
		if( *regtab[n] == '\0' ) return("<empty>");
		if( *regtab[n] == ' ' ) return("<erased>");
		return( regtab[n] );
	}
	return( "" );
}


char isdefault( int i )
{
	return( (i == default_screen)?'*':' ' );
}

bool has_emergency( int i )
{
	return false;
}

bool build_menu( void )
{
	int i;

	clear_menu();
	for( i=0; i<MAX_MENU_ROWS; i++ ) {
		sprintf( menu[i], "%s%1x%c%-16.16s%s  %s%1x%c%-16.16s%s",
			has_emergency(i*2)?"\x1b[25h":"",
			i*2, isdefault(i*2), reg_name(i*2),
			has_emergency(i*2)?"\x1b[25l":"",
			has_emergency(i*2+1)?"\x1b[25h":"",
			i*2+1, isdefault(i*2+1), reg_name(i*2+1),
			has_emergency(i*2+1)?"\x1b[25l":"" );
	}
	return( true );
}


void display( int fd, int row_index )
{
	int i, row;
	int menu_start_row = ((g_rows-3) < MAX_MENU_ROWS)?row_index:0;
	char buf[100];

	//build_menu();
	fpui_write_string_at(fd, "          FRONT PANEL MANAGER", 1, 1);
	fpui_write_string_at(fd, "SELECT WINDOW [0-F]  SET DEFAULT *[0-F]", 2, 1);
	for( i=menu_start_row, row=3; (i<MAX_MENU_ROWS) && (row<g_rows); i++ ) {
		sprintf(buf, "%1x%c%s%-16.16s%s  %1x%c%s%-16.16s%s",
			i*2, isdefault(i*2), has_emergency(i*2)?"\x1b[25h":"",
			reg_name(i*2),
			has_emergency(i*2)?"\x1b[25l":"",
			i*2+1, isdefault(i*2+1), has_emergency(i*2+1)?"\x1b[25h":"",
			reg_name(i*2+1),
			has_emergency(i*2+1)?"\x1b[25l":"" );
		fpui_write_string_at(fd, buf, row++, 1);
	}
	fpui_write_string_at(fd, "[UP/DN ARROW]        [CONFIG INFO- NEXT]", g_rows, 1);
	fpui_set_cursor_pos(fd, 3, 10);
}

void get_screen_size(int fd)
{
	char buf[1024];
	read_packet *rp = (read_packet *)buf;
	char type = 0;
	int count = 0;

	// Query panel type: A (4 lines); B (8 lines); D (16 lines)
	fpui_write_string( fd, "\x1b[c" );
	// response should be in read_packet format
	if( (count = read(fd, buf, sizeof(buf))) > 0 ) {
		if (rp->command == DATA) {
			if (sscanf((const char *)rp->data, "\x1b[%cR", &type) == 1) {
				if (toupper(type) == 'A')
					g_rows = 4;
				else if (toupper(type) == 'D')
					g_rows = 16;
			}
		}
	}
}

#define KEY_ENT   0x0d
#define KEY_SPLAT 0x2a
#define KEY_PLUS  0x2b
#define KEY_MINUS 0x2d

typedef enum { KEY_NONE=0x00, KEY_UP=0x80,  KEY_DOWN=0x81, KEY_RIGHT=0x82, KEY_LEFT=0x83,
	       KEY_NEXT=0x84,  KEY_YES=0x85, KEY_NO=0x86 } key_types;

struct {
	key_types   key;
	char      * seq;
} keymap[] = {
	{ KEY_UP,    "\x1b[A" },	// up arrow
	{ KEY_DOWN,  "\x1b[B" },	// down arrow
	{ KEY_RIGHT, "\x1b[C" },	// right arrow
	{ KEY_LEFT,  "\x1b[D" },	// left arrow
	{ KEY_NEXT,  "\x1bOP" },	// NEXT
	{ KEY_YES,   "\x1bOQ" },	// YES
	{ KEY_NO,    "\x1bOR" },	// NO
};
#define KEYMAP_SIZE (sizeof( keymap ) / sizeof( keymap[0] ))


int main( int argc, char * argv[] )
{
	int i;
	int row_index = 0;
	char buf[1024];
	read_packet *rp = (read_packet *)buf;
	bool default_cmd = false;
	int default_win = -1;
	char default_app[16] = "";

	signal( SIGHUP, alldone );
	signal( SIGWINCH, window_handler );

	// try to read the default app from /etc/default/fpui
	FILE *fp = fopen("/etc/default/fpui", "r");
	if (fp != NULL) {
		while (fgets(buf, 1024, fp)) {
			if( sscanf(buf, "DEFAULT_APP %s", default_app) == 1 ) {
				printf("MS: default app is %s\n", default_app);
				break;
			}
		};
		fclose(fp);
	}


	// initialize the array of Application Titles
	for( i = 0; i < APP_OPENS; i++ )
		regtab[i] = NULL;

	pad[0] = 0;

	//
	// NOTE when writing, we are writing X3.64 style strings to our virtual terminal
	//
	
	/*
	if( (fp = fopen( "/dev/msi", "wx" ) ) == NULL ) {
	        fprintf( stderr, "%s: Fopen error - %s\n", argv[0], strerror( errno ) );
		exit( 99 );
	}
	*/

	//
	// NOTE when reading, we are reading preformatted read_packets.
	//
	if( (msi = open( "/dev/msi", O_RDWR | O_EXCL )) < 0) {
	        fprintf( stderr, "%s: Open error - %s\n", argv[0], strerror( errno ) );
		exit( 99 );
	}

	printf("%s Ready msi=%d\n", argv[0], msi );

	// get actual panel dimensions
	get_screen_size( msi );
	fpui_clear( msi );
	display( msi, row_index );

	// Use default key mappings

	for( ;; ) {
		printf( "MS: reading on msi=%d\n", msi );
		if (((i = read(msi, buf, sizeof(buf))) < 0)
				|| (i < sizeof(read_packet)) ) {
			perror( argv[0] );
			exit( 30 );
		}
		switch( rp->command ) {
			case NOOP:
				printf("%s: NOOP packet from %d to %d\n", argv[0], rp->from, rp->to );
                                break;
			case DATA:
				printf("%s: DATA packet from %d to %d\n", argv[0], rp->from, rp->to );
				// Read each byte of "mapped" data from combined mapped+raw packet.
				for( i = 0; (i < rp->raw_offset) && (rp->data[i] != '\0'); i++ ) {
					switch( rp->data[i] ) {
						case KEY_NEXT:	{// to select the System Configuration screen
							unsigned int focus_dev = SC_DEV;
							fprintf( stderr, "MS: Setting focus to System Configuration\n");
							ioctl( msi, FP_IOC_SET_FOCUS, &focus_dev );
							break;
						}
						case KEY_UP:
							fprintf( stderr, "MS: keycode KEY_UP\n");
							if( (g_rows-3) < MAX_MENU_ROWS ) {
								if (row_index > 0) 
									row_index--;
							}
							break;
						case KEY_DOWN:
							fprintf( stderr, "MS: keycode KEY_DOWN\n");
							if( (g_rows-3) < MAX_MENU_ROWS ) {
								if ((row_index+(g_rows-3)) < MAX_MENU_ROWS)
									row_index++;
							}
							break;
						case KEY_SPLAT:	// '*"[0..F]<ENT> or '*'<ENT> to set the default screen
							fprintf( stderr, "MS: keycode *\n");
							default_cmd = true;
							default_win = -1;
							break;
						case KEY_ENT:
							if (default_cmd) {
								if (default_win == -1) {
									fprintf( stderr, "MS: setting MS_DEV as default screen\n" );
									default_screen = MS_DEV;
									strcpy(default_app, "");
									// remove /etc/default/fpui
									unlink("/etc/default/fpui");
								} else if (regtab[default_win] != NULL) {
									fprintf( stderr, "MS: setting %x as default screen\n", default_win );
									default_screen = default_win;
									strncpy(default_app, regtab[default_win], 15);
									default_app[15] = '\0';
									// save app name to /etc/default/fpui
									fp = fopen("/etc/default/fpui", "w");
									if (fp != NULL) {
										fprintf(fp, "DEFAULT_APP %.16s\n", default_app);
										fclose(fp);
									}
								} else {
									fprintf( stderr, "MS: Requested screen (%x) not active\n", default_win);	
								}
								default_cmd = false;
								break;
							}
							// invalid key
							// Issue "bell" alert to front panel
							char bel = 0x7;
							write(msi, &bel, 1);													
							break;
						default:	// [0..F] to select an application screen
						{
							fprintf( stderr, "MS: keycode 0x%2.2x\n", rp->data[i]);
							if (default_cmd) {
								if (isxdigit(rp->data[i])) {
									default_win = isdigit(rp->data[i])?
                                                                                (rp->data[i]-0x30):
                                                                                (toupper(rp->data[i])-55);
									break;
								}
							} else if(isxdigit(rp->data[i])) {
								int win = isdigit(rp->data[i])?
                                                                        (rp->data[i]-0x30):
                                                                        (toupper(rp->data[i])-55);
								if (regtab[win] != NULL) {
									fprintf( stderr, "MS: Setting focus to process %x\n", win);
									ioctl( msi, FP_IOC_SET_FOCUS, &win );
									break;
								}
							}
							// invalid selection
							fprintf( stderr, "MS: invalid keypress\n" );
							// Issue "bell" alert to front panel
							char bel = 0x7;
							write(msi, &bel, 1);						
							break;
						}
					}
				}
				display( msi, row_index );
                                break;
                        case CREATE:
				printf("%s: CREATE packet from %d to %d\n", argv[0], rp->from, rp->to );
                                break;
                        case REGISTER:
				regtab[rp->from] = calloc( 1, rp->size );
				strncpy( regtab[rp->from], (char *)rp->data, rp->size );
				printf("%s: REGISTER packet from %d to %d\n", argv[0], rp->from, rp->to );
				// If this is default app, switch to display it
				if (!strcmp(regtab[rp->from], default_app)) {
					default_screen = default_win = rp->from;
					fprintf( stderr, "MS: Setting focus to default app %s\n", default_app);
					ioctl( msi, FP_IOC_SET_FOCUS, &rp->from );
				}
				display( msi, row_index );
                                break;
                        case DESTROY:
				free( regtab[rp->from] );
				regtab[rp->from] = NULL;
				display( msi, row_index );
				printf("%s: DESTROY packet from %d to %d\n", argv[0], rp->from, rp->to );
                                break;
                        case FOCUS:
				printf("%s: FOCUS packet from %d to %d\n", argv[0], rp->from, rp->to );
                                break;
                        case EMERGENCY:
				// set emergency state for this app according to packet data
				printf("%s: EMERGENCY packet from %d to %d\n", argv[0], rp->from, rp->to );
                                break;				
                        default:
				printf("%s: Undefined packet from %d to %d\n", argv[0], rp->from, rp->to );
				exit( 99 );
                                break;

		}
	}
}
