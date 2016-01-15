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
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>
#include <fcntl.h>
#include <signal.h>
#include <errno.h>
#include <string.h>
#include <ctype.h>
#include <regex.h>
#include <pthread.h>
#include <termios.h>
#include <sys/time.h>
#include <sys/poll.h>

#include <front_panel.h>

#undef DEBUG

#ifdef DEBUG
#define DBG(x...)           printf(x);
#else
#define DBG(x...)           do{}while(0)
#endif

#define ESC "\x1b"
#define CHAR_ESC '\x1b'


void set_focus( int);
int get_focus( void );
void send_to_current( char );
void viewport_listener( char * );


int panel_fd = -1;
bool display_present = false;

extern void vt_lock( int );
extern void load_screen( int, int );
extern void vt_unlock( int );
extern void tohex( char * );
extern void routing_send_signal( int );
extern void routing_return(int, char *, char *s);
extern int is_active( int );
extern void virtual_terminal_return( int, char * );

#define REGEX_INIT { .buffer = NULL }

// special strings
struct {
	char * pattern;
	regex_t preg;
} special_string[] = {
	{ ESC "OS", REGEX_INIT },
	{ ESC "OT", REGEX_INIT },
	{ ESC "OU", REGEX_INIT },
	{ ESC "OP", REGEX_INIT },
	{ ESC "[[]PU", REGEX_INIT },
	{ ESC "[[][hl]R", REGEX_INIT },
	{ ESC "[[][0-9]+;[0-9]+R", REGEX_INIT },
	{ ESC "[[][hl];[hl];[hl];[hl];[0-9]+;[hl]R", REGEX_INIT },
	{ ESC "[[][ABD]R", REGEX_INIT },
};

typedef enum { ESC_KEY, AUXSW_ON, AUXSW_OFF, NXT_KEY, PWR_UP, AUX_STATE, CUR_POS, STATUS, FP_TYPE } special_string_types;

#define SPECIAL_STRING_SIZE ( sizeof( special_string ) / sizeof( special_string[0] ) )


int has_focus = MS_DEV;

void set_focus( int term )
{
	DBG("%s: setting focus to device %d\n", __func__, term );
	// signal process has_focus
	has_focus = term;
	
	vt_lock( term );
	load_screen( panel_fd, term );
	vt_unlock( term );
}

int get_focus( void )
{
	return has_focus;
}

bool panel_present( void )
{
	return display_present;
}

int screen_YY = 16;
int screen_XX = 40;

bool check_screen_size( int fd )
{
	int i, t, row, col;

	struct pollfd ufds;
	ufds.fd      = fd;
	ufds.events  = POLLIN;
	ufds.revents = 0;

	// Set Default values
	screen_YY = 16;
	screen_XX = 40;

	// to determine the screen size, one idea is to set the cursor position
	// to a position that is off the screen,
	// and to read back the cursor position which should be at the bottom row of the screen
	const int numScreenSizes = 4;
	const int screenSizes[4][2] = {{80, 40}, {40, 16}, {40, 8}, {40, 4}};
	char outbuf[40], inbuf[40];
	for (t = 0; t < numScreenSizes; t++) 
	{
		// write cursor position		
		row = screenSizes[t][1];
		col = screenSizes[t][0];
		DBG("Setting cursor at %d, %d\n", row, col);
		sprintf( outbuf, "%c[%d;%df", CHAR_ESC, row, col );
		write( fd, outbuf, strlen(outbuf) );
		
		// Read cursor position
		row = 0;
		col = 0;
		sprintf( outbuf, "%c[6n", CHAR_ESC );
		write( fd, outbuf, strlen(outbuf) );
		if(poll( &ufds, 1, 200) == 1) {
			if( (i = read( fd, inbuf, 40 )) > 0 ) {
				inbuf[i] = '\0';
				i = sscanf( inbuf, ESC "[%d;%dR", &row, &col ); // read cursor position
			}
		} else {
			return false;
		}

		// check if cursor position matches
		if ((row == screenSizes[t][1]) && (col == screenSizes[t][0])) {
			screen_YY = row;
			screen_XX = col;
			/*DBG*/printf("screen size detected %d %d\n", row, col);
			break;
		}
	}
	DBG( "%s: i=%d, screen_YY=%d, screen_XX=%d\n", __func__, i, screen_YY, screen_XX );
	return true;
}

int get_screen_type( char *type )
{
	switch (screen_YY) {
	case 4: *type = 'A';
		break;
	case 8: *type = 'B';
		break;
	case 16: *type = 'D';
		break;
	default: return -1;
	}
	return 0; 
}


/*
	Requirement 3.3.1.2-cc
	This requirement states that the front panel must be polled every 5 seconds to
	determine wether or not it has been removed and if it's size has changed.

	Implementation
	Upon detecting either of these events, the new state is logged and the SIGWINCH
	signal is sent to all registered applications. If the application chooses to 
	listen for this signal, it can then query the FPM for the new state of the panel.

	Every attempt is made to separate normal traffic from this poll but there is a 
	possability that a message or response will bet garbled.

	The get cursor position query is <ESC>[6n
	the response should be <ESC>[yy;xxR where yy and xx are ASCII encoded decimal digits.
*/


//
// This routine runs as a LWP thread. It listenes on the
// serial in port for keystrokes. It checks for a few special
// ones and passes the rest the the virtual terminal that
// currently has focus.
//
// The key sequence '*' '*' 'ESC' will always return focus
// to the Master Selection Virtual Terminal. This has the
// actual code 0x2a 0x2a 0x1b 0x4f 0x53. The time between
// characters can not exceed 1Sec or any characters received
// so far are sent to the current virtual terminal and the
// special string must be re-entered
//


//
// This routine listens for key strokes from the Physical Front Panel device.
// When a character is received, it is first compared to the special string
// which forces the Master Selection Manager into focus. If the character is
// not part of this string, it is sent to the device which currently has focus.
// If the character happens to match the current position in the special string,
// it is withheld and the index into the special string is advanced. This
// continues until either a character not in the special string is received,
// a timeout occures or the entire special string is received. If a character
// not in the special string is received or a timeout occured, the index into
// the special string is used to send all withheld characters to the device
// currently in focus (along with the non-matching character). If the special
// string is received, focus is immediatly given to the Master Selection Manager.
//

void viewport_cleanup( void *arg )
{
	if (panel_fd >= 0)
		close(panel_fd);
	DBG( "viewport_listener - cleanup\n");
}


void parse_escape_seq( int fd, char *buf, int len )
{
	int  i  = 0;
	char ch = ' ';
	struct pollfd ufds;
	ufds.fd      = fd;
	ufds.events  = POLLIN;
	ufds.revents = 0;

	// we have already received the escape character
	// now we have to receive the rest.
	// Assumptions:
	//	All incoming escape sequences are at least 3 characters long
	//	Sequences of the form <ESC> [ [0-9] are terminated with an 'R' character.
	//	There are no other esc
	buf[0] = CHAR_ESC;
	read(fd,&ch,1);
	buf[1] = ch;
	read(fd,&ch,1);
	buf[2] = ch;
	buf[3] = '\0';

	if (buf[1] != '[')
		goto out;

	switch (buf[2]) {
	case 'A': case 'B': case 'C': case 'D':
		if (poll(&ufds, 1, 0) != 1)
			goto out;
		read(fd,&ch,1);
		if (ch == 'R') {
			buf[3] = ch;
			buf[4] = '\0';
		}
		goto out;
		break;
	case 'P':
		read(fd,&ch,1);
		buf[3] = ch;
		buf[4] = '\0';
		goto out;
		break;
	case 'h': case 'l':
		break;
	default:
		if (!isdigit(buf[2]))
			goto out;
		break;
	}

	for( i = 3; (ch != 'R') && (i < (len-1)); i++ ) {
		read(fd,&ch,1);
		buf[i] = ch;
	}
	buf[i] = '\0';
out:
	DBG("%s: %02x %02x %02x %02x\n", __func__, buf[0], buf[1], buf[2], buf[3]);
}



#define NO_TIMEOUT    -1
#define ZERO_TIMEOUT  0
#define LONG_TIMEOUT  5000
#define SHORT_TIMEOUT 1000

static bool ping   = false;

void viewport_listener( char *filepath )
{
	char		ch = '\0';
	char		buf[32];
	int		fd = -1;
	struct pollfd	ufds;
	int 		state = 0;
	int		timeout = LONG_TIMEOUT;
	int		i;
	int		errcode;
	struct termios port_attr;

	pthread_cleanup_push( viewport_cleanup, NULL );
	pthread_detach( pthread_self() );

	DBG( "%s: Starting on port %s\n", __func__, filepath );

	// prepare the regular expression pattern buffers for the special strings.
	for( i = 0; i < SPECIAL_STRING_SIZE; i++ ) {
		if( (errcode = regcomp( &special_string[i].preg, special_string[i].pattern, REG_EXTENDED | REG_NOSUB )) != REG_NOERROR ) {
			char errbuf[128];
			regerror( errcode, &special_string[i].preg, errbuf, sizeof( errbuf ) );
			fprintf( stderr, "%s: regcomp failed with > %s\n", __func__, errbuf );
		}
	}

	// main processing loop
	for( ;; ) {

		DBG("Opening device port %s\n", filepath );
		if( (fd = open(filepath,O_RDWR|O_NOCTTY)/*fopen( filepath, "r+" )*/) < 0 ) {
			fprintf( stderr, "%s: Failed to fopen %s - %s\n", __func__, filepath, strerror( errno ) );
			pthread_exit( NULL );
		}/*else if( (fd = fileno( panel )) == -1 ) {
			fprintf( stderr, "%s: Failed to convert file descriptor %s - %s\n", __func__, filepath, strerror( errno ) );
			pthread_exit( NULL );
		}*/
		panel_fd = fd;
		
		/* Set port attributes */
		//tcgetattr(fd,&port_attr);
		port_attr.c_cflag = B38400|CS8|CLOCAL|CREAD;
		port_attr.c_iflag = IGNBRK|IGNPAR;
		port_attr.c_oflag = OPOST|ONLCR;
		port_attr.c_lflag = 0;
		port_attr.c_cc[VTIME] = 0;
		port_attr.c_cc[VMIN] = 0;
		tcflush(fd,TCIFLUSH);
		tcsetattr(fd,TCSANOW,&port_attr);

		display_present = check_screen_size( fd );
		
		DBG( "%s listening on %s\n", __func__, filepath );

		ufds.fd      = fd;
		ufds.events  = POLLIN;
		ufds.revents = 0;

		for( ;; ) {
//fprintf( stderr, "%s: poll with timeout %d\n", __func__, timeout );
			switch( poll( &ufds, 1, timeout ) ) {
				case -1:
					fprintf( stderr, "Poll failed with %s\n", strerror( errno ) );
					pthread_exit( NULL );
				case 0:

					if( timeout == LONG_TIMEOUT ) {
						//
						// do 5 second things.
						//
						if( ping ) {	// did we send a ping and not get a response
							//screen_YY = 0;
							//screen_XX = 0;
							if( display_present ) {
								display_present = false;
								/*DBG*/printf( "%s: Display has been disconnected.\n", __func__ );
								// TODO we need to signal the change
								routing_send_signal(FP_MAX_DEVS);
							}
						}
                                                // Send ping (now enquire AUX switch state)
						write( fd, ESC "[An", 4 );
						ping = true;
					} else if( timeout == SHORT_TIMEOUT ) {
						//
						// do 1 second things.
						//
						if( state == 1 ) {							// did we get one '*' and then timeout
							sprintf( buf, "*" );						//   prepare a string buffer
							virtual_terminal_return( has_focus, buf );			//   send the string to the virtual terminals
							state = 0;
						}
						// NOTE: there is no requirement for the anount of time between '**' and the <ESC> key
						// so if we are in state 2 (we got '**' so far) we can only go back to normal polling
						// and hope that the operator eventually presses another key. But if there were a requirement
						// NOTE: Comments on this have indicated a desire to implement a timeout between the second '*'
						// and the <ESC> key.
						if( state == 2 ) {						// did we get '**' and then timeout
							sprintf( buf, "**" );					//   prepare a string buffer
							virtual_terminal_return( has_focus, buf );		//   send the string to the virtual terminals
							state = 0;
						}
					}
					timeout = LONG_TIMEOUT;		

					break;

				default:	// this will probably be '1'
					if( (ufds.revents & POLLIN) == 0 ) {
						break;
					} else if( read(fd, &ch, 1) != 1 ) {		// get a character
						fprintf( stderr, "%s: Fgetc EOF (%s)\n", __func__, strerror( errno ) );
					} else if (!display_present) {
						// Panel must be present now
						/*DBG*/printf( "%s: Display has been reconnected.\n", __func__ );
						display_present = true;
						tcflush(fd,TCIFLUSH);
						check_screen_size( fd );
						set_focus(has_focus);
						routing_send_signal(FP_MAX_DEVS);
						ping = false;
						break;
					} else if( ch == CHAR_ESC ) {
						// get as much of the escape sequence as we can recognize
						parse_escape_seq( fd, buf, sizeof( buf ) );

						// compare the sequence to the list of special strings
						for( i = 0; i < SPECIAL_STRING_SIZE; i++ ) {
							if( (errcode = regexec( &special_string[i].preg, buf, 0, NULL, 0 )) == REG_NOERROR ) {
								break;
							}
						}

						// finally handle the special string, or pass any other string up thre channel.
						switch( i ) {
							case ESC_KEY:	// the Escape button from the Front Panel
								if( state == 1 ) {					// we got one '*' and the <ESC> key
									sprintf( buf, "*" );				//   prepare a string buffer
									virtual_terminal_return( has_focus, buf );	//   send the string to the virtual terminals
									sprintf( buf, ESC "OS" );			//   prepare a string buffer
									virtual_terminal_return( has_focus, buf );	//   send the string to the virtual terminals
								} else if( state == 2 ) {				// we got two '*'s and the <ESC> key
									set_focus( MS_DEV );				//   set the MS_DEV to focus
									DBG( "%s: MS focus sequence\n", __func__ );
								} else {						// we got an isolated <ESC> key
									sprintf( buf, ESC "OS" );			//   prepare a string buffer
									virtual_terminal_return( has_focus, buf );	//   send the string to the virtual terminals
								}
								break;
							case NXT_KEY:
								if( state == 2 ) {				    // we got two '*'s and the <NEXT> key
									if (is_active(SC_DEV)) {
										set_focus( SC_DEV );				//   set the SC_DEV to focus
										DBG( "%s: SC focus sequence\n", __func__ );
										// also send ESC to SC in case in submenu
										sprintf(buf, ESC "OS");
										virtual_terminal_return( has_focus, buf );	//   send the string to the virtual terminals
									}
								} else if( state == 1 ) {			    // we got one '*' and the <NEXT> key
									    sprintf( buf, "*" );			//   prepare a string buffer
									    virtual_terminal_return( has_focus, buf );	//   send the string to the virtual terminals
								} else if( has_focus == SC_DEV ){
									set_focus(MS_DEV);				// isolated <NEXT> in SC_DEV
									DBG( "%s: MS focus sequence\n", __func__ );
								} else {
									sprintf( buf, ESC "OP" );			//   prepare a string buffer
									virtual_terminal_return( has_focus, buf );	//   send the string to the virtual terminals
								}
								break;
							case AUXSW_ON:	// the AUX switch has been turned ON
							{
								char tmpbuf[4];
								tmpbuf[0] = AUX_SWITCH_ON; tmpbuf[1] = '\0';
								routing_return( AUX_DEV, tmpbuf, NULL );
								DBG( "%s: AUX ON sequence\n", __func__ );
								//virtual_terminal_return( has_focus, buf );
								break;
							}
							case AUXSW_OFF:	// the AUX switch has been turned OFF
							{
								char tmpbuf[4];
								tmpbuf[0] = AUX_SWITCH_OFF; tmpbuf[1] = '\0';
								routing_return( AUX_DEV, tmpbuf, NULL );
								DBG( "%s: AUX OFF sequence\n", __func__ );
								//virtual_terminal_return( has_focus, buf );
								break;
							}
							case AUX_STATE: // response to a query for AUX switch state
								DBG( "%s: AUX switch status sequence\n", __func__ );
								if( ping ) {
									if( !display_present ) {
										/*DBG*/printf( "%s: Display has been reconnected (ping).\n", __func__ );
										// TODO we need to signal the change
										display_present = true;
										check_screen_size( fd );
										routing_send_signal(FP_MAX_DEVS); //signal all
										set_focus(has_focus);
									}
									ping = false;
								}
                                                                // read AUX switch state and send status packet to AUX_DEV
                                                                if (buf[2] == 'h') {
									char tmpbuf[4];
									tmpbuf[0] = AUX_SWITCH_ON; tmpbuf[1] = '\0';
									routing_return(AUX_DEV, tmpbuf, NULL);
								} else if (buf[2] == 'l') {
									char tmpbuf[4];
									tmpbuf[0] = AUX_SWITCH_OFF; tmpbuf[1] = '\0';
									routing_return(AUX_DEV, tmpbuf, NULL);
								}									
								//virtual_terminal_return( has_focus, buf );
								break;
							case PWR_UP:	// The UI powered up or reset
								DBG( "%s: POWER UP sequence\n", __func__ );
								virtual_terminal_return( has_focus, buf );
								break;
							case STATUS: //Status at cursor position
								DBG( "%s: Status Cursor Position response\n", __func__);
								routing_return( has_focus, NULL, buf );
								break;
							case FP_TYPE: //Panel Type Response
								DBG( "%s: Front Panel Type sequence\n", __func__ );
								// Ignore for now, as panel size determined differently
								break;
							case CUR_POS:	// the response to a 'get cursor position' inquiry.
								DBG( "%s: CURSOR POS sequence\n", __func__ );
								routing_return( has_focus, NULL, buf );
								break;
							default:
								DBG( "%s: Unrecognized sequence\n", __func__ );
								// send the string to the virtual terminals
								virtual_terminal_return( has_focus, buf );
								break;
						}
						state = 0;
					} else if( ch == '*' ) {
						switch( state ) {
							case 0:	// we received a '*' character, shorten the timeout for the next one.
								timeout = SHORT_TIMEOUT;						//   reset the poll timeout for 1 second
								state = 1;						//   go look for the second *
								// the time out handler for poll (above) will take care of sending
								// the single '*' if a time out occures before the next '*' arrives.
								break;
							case 1:		// we received the second '*' within the allowed time
								timeout = SHORT_TIMEOUT;						//   keep the poll timeout at 1 second
								state = 2;
								// now we defer to the escape sequence detection and time out
								// handler above to finalize this operation.
								break;
						}
					} else {						// any other character gets passed on up the channel
						if( state == 1 ) {				// we received a '*' followed by some other character
							sprintf( buf, "*%c", ch );		//   prepare a string buffer
						} else if( state == 2 ) {			// we received a '**' followed by some other character
							// if {**,[0-F]} then set focus to app?
							if (isxdigit(ch)) {
                                                                char item = isdigit(ch)?(ch-0x30):(toupper(ch)-55);
                                                                if (is_active(item)) {
                                                                        set_focus(item);
                                                                        break;
                                                                }
                                                        }
                                                        sprintf( buf, "**%c", ch );	// prepare a string buffer
						} else {					// we received a character without any special meaning
							sprintf( buf, "%c", ch );		//   prepare a string buffer
						}
						virtual_terminal_return( has_focus, buf );	// send the string to the virtual terminals
						state = 0;
					}
			}
		}
	}
	pthread_cleanup_pop( 1 );
}


void viewport_copy_out( int term, char *s )
{
	//int fd = fileno(panel);
	if( term == has_focus ) {
		write( panel_fd, s, strlen(s)/*+1*/ );
		DBG( "%s: done\n", __func__ );
	} else {
		DBG( "%s: Terminal %d does not have focus\n", __func__, term );
	}
}


