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

#ifndef ATC_SC_SCREEN_H
#define ATC_SC_SCREEN_H

// Internal and external screens max size info
#define EXTERNAL_SCREEN_X_SIZE 	40
#define EXTERNAL_SCREEN_Y_SIZE 	16  // in reality its 8 but the last line is reserved for "Quick Help"

#define MAX_INTERNAL_SCREEN_X_SIZE 	40
#define MAX_INTERNAL_SCREEN_Y_SIZE 	80

#define MAX_FIELDS_PER_LINE 		20
#define MAX_FIELD_STRING_VALUES		40


// ID for the various internal screens
#define NO_INTERNAL_SCREENS 	9
#define MENU_SCREEN_ID      0
#define TIME_SCREEN_ID		1
#define ETH1_SCREEN_ID      2
#define ETH2_SCREEN_ID      3
#define SRVC_SCREEN_ID      4
#define LNUX_SCREEN_ID      5
#define APIV_SCREEN_ID      6
#define EPRM_SCREEN_ID		7
#define HELP_SCREEN_ID		8

// Sizes of various internal screens
#define MENU_SCREEN_X_SIZE 	40
#define MENU_SCREEN_Y_SIZE 	11
#define MENU_SCREEN_HEADER_SIZE	 2
#define TIME_SCREEN_X_SIZE 	40
#define TIME_SCREEN_Y_SIZE 	 8
#define TIME_SCREEN_HEADER_SIZE	 1
#define ETH1_SCREEN_X_SIZE 	40
#define ETH1_SCREEN_Y_SIZE 	11
#define ETH2_SCREEN_X_SIZE 	40
#define ETH2_SCREEN_Y_SIZE 	11
#define ETH_SCREEN_HEADER_SIZE	 2
#define SRVC_SCREEN_X_SIZE 	40
#define SRVC_SCREEN_Y_SIZE 	 8
#define SRVC_SCREEN_HEADER_SIZE	 2
#define LNUX_SCREEN_X_SIZE 	40
#define LNUX_SCREEN_Y_SIZE 	 8
#define LNUX_SCREEN_HEADER_SIZE	 1
#define APIV_SCREEN_X_SIZE 	40
#define APIV_SCREEN_Y_SIZE 	 8
#define APIV_SCREEN_HEADER_SIZE	 1
#define EPRM_SCREEN_X_SIZE 	40
#define EPRM_SCREEN_Y_SIZE 	 8
#define EPRM_SCREEN_HEADER_SIZE	 1
#define HELP_SCREEN_X_SIZE 	40
#define HELP_SCREEN_Y_SIZE 	 8
#define HELP_SCREEN_HEADER_SIZE	 1

// Definitions for the CONFIG MENU screen
#define HEADER_LINE_0_MENU	"     ATC CONFIGURATION INFORMATION      "
#define HEADER_LINE_1_MENU	" SELECT ITEM [0-F]                      "
#define LINE_0_MENU		"0 System Time       1 Ethernet 1 Config "
#define LINE_1_MENU		"2 Ethernet 2 Config 3 System Services   "
#define LINE_2_MENU		"4 Linux Info        5 API Info          "
#define LINE_3_MENU		"6 Host EEPROM Info  7                   "
#define LINE_4_MENU		"8                   9                   "
#define LINE_5_MENU		"A                   B                   "
#define LINE_6_MENU		"C                   D                   "
#define LINE_7_MENU		"E                   F                   "
#define FOOTER_LINE_MENU	"[UP/DN ARROW]        [FRONT PANEL- NEXT]"

// Definitions for the CONFIG TIME screen
#define HEADER_LINE_0_TIME 	"              SYSTEM TIME               "
#define LINE_0_TIME		"DATE       TIME     TMZONE DST/Status   "
#define LINE_1_TIME 		"  /  /     00:00:00 +00:00 Disabl/      "
#define LINE_2_TIME 		"                                        "
#define LINE_3_TIME 		"                CHANGE                  "
#define LINE_4_TIME 		"DATE       TIME     TMZONE DST          "
#define LINE_5_TIME 		"  /  /     00:00:00 +00:00 Disabl       "
#define FOOTER_LINE_TIME 	"[UP/DN ARROW] [APPLY-ENT]  [QUIT-**NEXT]"

#define DATE_TIME_DISP_LINE	1
#define DATE_TIME_EDIT_LINE	5
#define DATE_MONTH_FIELD 	0
#define DATE_DAY_FIELD 		2
#define DATE_YEAR_FIELD		4
#define TIME_HOUR_FIELD		6
#define TIME_MIN_FIELD		8
#define ZONE_SIGN_FIELD     10
#define ZONE_HOUR_FIELD     11
#define ZONE_MIN_FIELD      13
#define DST_ENABLE_FIELD    15

// Definitions for the CONFIG ETH1 screen
#define HEADER_LINE_0_ETH	"         ETHERNET CONFIGURATION         "
#define HEADER_LINE_1_ETH	"  ETHERNET PORT 1 (00:00:00:00:00:00)   "
#define LINE_0_ETH  		"Port Mode:       disabled               "
#define LINE_1_ETH  		"IP Address:        0.  0.  0.  0        "
#define LINE_2_ETH  		"Subnet Mask:       0.  0.  0.  0        "
#define LINE_3_ETH  		"Default Gateway:   0.  0.  0.  0        "
#define LINE_4_ETH  		"DNS Server:        0.  0.  0.  0        "
#define LINE_5_ETH  		"Host Name:                              "
#define LINE_6_ETH  		"Packets Sent GD:           BD:          "
#define LINE_7_ETH  		"Packets Rcvd GD:           BD:          "
#define FOOTER_LINE_ETH 	"[UP/DN ARROW] [APPLY-ENT]  [QUIT-**NEXT]"
#define ETH_MODE_LINE 0
#define ETH_MODE_FIELD 1
#define ETH_IPADDR_LINE 1
#define ETH_IPADDR1_FIELD 1
#define ETH_IPADDR2_FIELD 3
#define ETH_IPADDR3_FIELD 5
#define ETH_IPADDR4_FIELD 7
#define ETH_NETMASK_LINE 2
#define ETH_NETMASK1_FIELD 1
#define ETH_NETMASK2_FIELD 3
#define ETH_NETMASK3_FIELD 5
#define ETH_NETMASK4_FIELD 7
#define ETH_GWADDR_LINE 3
#define ETH_GWADDR1_FIELD 1
#define ETH_GWADDR2_FIELD 3
#define ETH_GWADDR3_FIELD 5
#define ETH_GWADDR4_FIELD 7
#define ETH_NSADDR_LINE 4
#define ETH_NSADDR1_FIELD 1
#define ETH_NSADDR2_FIELD 3
#define ETH_NSADDR3_FIELD 5
#define ETH_NSADDR4_FIELD 7
#define ETH_HOSTNAME_LINE 5
#define ETH_HOSTNAME_FIELD1 1
#define ETH_HOSTNAME_FIELDS 16
#define ETH_TX_LINE 6
#define ETH_RX_LINE 7

// Definitions for the CONFIG ETH2 screen
#define HEADER_LINE_1_ETH2	"  ETHERNET PORT 2 (00:00:00:00:00:00)   "


// Definitions for the SYSTEM SERVICES screen
#define HEADER_LINE_0_SRVC	"     SYSTEM SERVICES CONFIGURATION      "
#define HEADER_LINE_1_SRVC	"SERVICE                STATUS   CHANGE  "
#define FOOTER_LINE_SRVC	"[UP/DN ARROW] [APPLY-ENT]  [QUIT-**NEXT]"
#define SRVC_NAME_FIELD 0
#define SRVC_ENABLE_FIELD 2

// Definitions for the LINUX INFO screen
#define HEADER_LINE_0_LNUX	"           LINUX INFORMATION            "
#define LINE_0_LNUX		"Linux Release:                          "
#define LINE_1_LNUX		"Linux Version:                          "
#define LINE_2_LNUX		"Machine Hardware Type:                  "
#define LINE_3_LNUX		"Total Memory:                           "
#define LINE_4_LNUX		"Uptime:                                 "
#define LINE_5_LNUX		"Load Average:                           "
#define FOOTER_LINE_ETH 	"[UP/DN ARROW] [APPLY-ENT]  [QUIT-**NEXT]"
#define LNX_UPT_LINE	4
#define LNX_AVG_LINE	5

// Definitions for the API INFO screen
#define HEADER_LINE_0_APIV	"            API INFORMATION             "
#define LINE_0_APIV		"API Version:                            "
#define LINE_1_APIV		"API Driver Version:                     "
#define LINE_2_APIV		"API Version:                            "
#define LINE_3_APIV		"API Driver Version:                     "
#define LINE_4_APIV		"API Version:                            "
#define LINE_5_APIV		"                                        "
#define FOOTER_LINE_APIV 	"[UP/DN ARROW] [APPLY-ENT]  [QUIT-**NEXT]"

// Definitions for the HOST EEPROM screen
#define HEADER_LINE_0_EPRM	"        HOST EEPROM INFORMATION         "
#define LINE_0_EPRM		"Host EEPROM Version:                    "
#define LINE_1_EPRM		"Host EEPROM Size (bytes):               "
#define LINE_2_EPRM		"                                        "
#define LINE_3_EPRM		"                                        "
#define LINE_4_EPRM		"                                        "
#define LINE_5_EPRM		"                                        "
#define FOOTER_LINE_EPRM 	"[UP/DN ARROW] [APPLY-ENT]  [QUIT-**NEXT]"

// Definitions for the HELP screen
#define HEADER_LINE_0_HELP	"           HELP SCREEN                  "
#define LINE_0_HELP		"0-9              CHANGE FIELD VALUES    "
#define LINE_1_HELP		"+/-              ADJUST FIELD VALUES    "
#define LINE_2_HELP		"ARROW UP/DOWN    SCROLL UP/DOWN         "
#define LINE_3_HELP		"ARROW RIGHT/LEFT GO NEXT/PREVIOUS FIELD "
#define LINE_4_HELP		"ENT/YES          COMMIT FIELD CHANGE    "
#define LINE_5_HELP		"NO               REVERT FIELD CHANGE    "
#define FOOTER_LINE_HELP	"[MORE: +/-]                             "

#define CS_DISPLAY_MAKE_BLINKING	0x01
#define CS_DISPLAY_STOP_BLINKING	0x02
#define CS_DISPLAY_SET_CURSOR		0x03

#endif //#define ATC_SC_SCREEN_H
