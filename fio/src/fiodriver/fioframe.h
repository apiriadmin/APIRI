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

/*****************************************************************************/
/*

This file contains all definitions for the FIOMAN.

*/
/*****************************************************************************/

#ifndef _FIOFRAME_H_
#define _FIOFRAME_H_

/*  Include section.
-----------------------------------------------------------------------------*/

/* System includes. */


/* Local includes. */
#include "fioman.h"

/*  Definition section.
-----------------------------------------------------------------------------*/
/*  Module public structure/enum definition.*/

/* Frame #defines */
#define FIOMAN_MAX_TX_FRAME_SIZE	( 1024 )
#define FIOMAN_MAX_RX_FRAME_SIZE	( 1024 )

/* NEMA TS2 Frames */
#define FIOMAN_FRAME_NO_0			(0)
#define FIOMAN_FRAME_NO_0_SIZE		(16)
#define FIOMAN_FRAME_NO_1			(1)
#define FIOMAN_FRAME_NO_1_SIZE		(3)
#define FIOMAN_FRAME_NO_3			(3)
#define FIOMAN_FRAME_NO_3_SIZE		(3)
#define FIOMAN_FRAME_NO_9			(9)
#define FIOMAN_FRAME_NO_9_SIZE		(12)
#define FIOMAN_FRAME_NO_10			(10)
#define FIOMAN_FRAME_NO_11			(11)
#define FIOMAN_FRAME_NO_10_SIZE		(11)	/* addr + ctrl + frame 10 def */
#define FIOMAN_FRAME_NO_12			(12)
#define FIOMAN_FRAME_NO_13			(13)
#define FIOMAN_FRAME_NO_12_SIZE		(8)	/* addr + ctrl + frame 12 def */
#define FIOMAN_FRAME_NO_18			(18)
#define FIOMAN_FRAME_NO_18_SIZE		(3)
#define FIOMAN_FRAME_NO_20			(20)
#define FIOMAN_FRAME_NO_21			(21)
#define FIOMAN_FRAME_NO_22			(22)
#define FIOMAN_FRAME_NO_23			(23)
#define FIOMAN_FRAME_NO_20_SIZE		(3)
#define FIOMAN_FRAME_NO_24			(24)
#define FIOMAN_FRAME_NO_25			(25)
#define FIOMAN_FRAME_NO_26			(26)
#define FIOMAN_FRAME_NO_27			(27)
#define FIOMAN_FRAME_NO_24_SIZE		(4)

#define FIOMAN_FRAME_NO_128			(128)
#define FIOMAN_FRAME_NO_128_SIZE	(3)
#define FIOMAN_FRAME_NO_129			(129)
#define FIOMAN_FRAME_NO_129_SIZE	(14)
#define FIOMAN_FRAME_NO_131			(131)
#define FIOMAN_FRAME_NO_131_SIZE	(23)
#define FIOMAN_FRAME_NO_138			(138)
#define FIOMAN_FRAME_NO_139			(139)
#define FIOMAN_FRAME_NO_140			(140)
#define FIOMAN_FRAME_NO_141			(141)
#define FIOMAN_FRAME_NO_138_SIZE	(8)
#define FIOMAN_FRAME_NO_148			(148)
#define FIOMAN_FRAME_NO_149			(149)
#define FIOMAN_FRAME_NO_150			(150)
#define FIOMAN_FRAME_NO_151			(151)
#define FIOMAN_FRAME_NO_148_SIZE	(37)
#define FIOMAN_FRAME_NO_152			(152)
#define FIOMAN_FRAME_NO_153			(153)
#define FIOMAN_FRAME_NO_154			(154)
#define FIOMAN_FRAME_NO_155			(155)
#define FIOMAN_FRAME_NO_152_SIZE	(19)
/* CalTrans/ITS/NEMA TS1 Frames */
#define	FIOMAN_FRAME_NO_49			( 49 )
#define	FIOMAN_FRAME_NO_49_SIZE		( 4 )	/* addr + ctrl + frame 49 def */
#define	FIOMAN_FRAME_NO_51			( 51 )
#define	FIOMAN_FRAME_NO_51_SIZE		( 196 )	/* addr + ctrl + frame 51 def */
#define	FIOMAN_FRAME_NO_52			( 52 )
#define	FIOMAN_FRAME_NO_52_SIZE		( 3 )	/* addr + ctrl + frame 52 def */
#define	FIOMAN_FRAME_NO_53			( 53 )
#define	FIOMAN_FRAME_NO_53_SIZE		( 3 )	/* addr + ctrl + frame 53 def */
#define	FIOMAN_FRAME_NO_54			( 54 )
#define	FIOMAN_FRAME_NO_54_SIZE		( 4 )	/* addr + ctrl + frame 54 def */
#define FIOMAN_FRAME_NO_55			( 55 )
#define FIOMAN_FRAME_NO_55_SIZE		( 19 )	/* addr + ctrl + frame 55 def */
#define FIOMAN_FRAME_NO_60			( 60 )
#define FIOMAN_FRAME_NO_60_SIZE		( 3 )	/* addr + ctrl + frame 60 def */
#define FIOMAN_FRAME_NO_61			( 61 )
#define FIOMAN_FRAME_NO_61_SIZE		( 16 )	/* addr + ctrl + frame 61 def */
#define	FIOMAN_FRAME_NO_62			( 62 )
#define	FIOMAN_FRAME_NO_62_SIZE		( 4 )	/* addr + ctrl + frame 62 def */
#define FIOMAN_FRAME_NO_65			( 65 )
#define FIOMAN_FRAME_NO_65_SIZE		( 3 )	/* addr + ctrl + frame 65 def */
#define	FIOMAN_FRAME_NO_66			( 66 )
#define	FIOMAN_FRAME_NO_66_SIZE		( 10 )	/* addr + ctrl + frame 66 def */
#define	FIOMAN_FRAME_NO_67			( 67 )
#define	FIOMAN_FRAME_NO_67_SIZE		( 16 )	/* addr + ctrl + frame 67 def */

#define	FIOMAN_FRAME_NO_177			( 177 )
#define	FIOMAN_FRAME_NO_177_SIZE	( 10 )	/* addr + ctrl + frame 177 def */
#define	FIOMAN_FRAME_NO_179			( 179 )
#define FIOMAN_FRAME_NO_179_SIZE	( 4 )	/* addr + ctrl + frame 179 def */
#define	FIOMAN_FRAME_NO_180			( 180 )
#define	FIOMAN_FRAME_NO_180_SIZE	( 22 )	/* addr + ctrl + frame 180 def */
#define	FIOMAN_FRAME_NO_181			( 181 )
#define	FIOMAN_FRAME_NO_181_SIZE	( 22 )	/* addr + ctrl + frame 181 def */
#define	FIOMAN_FRAME_NO_182			( 182 )
#define	FIOMAN_FRAME_NO_182_SIZE	( 775 )	/* addr + ctrl + frame 182 def */
#define FIOMAN_FRAME_NO_183			( 183 )
#define FIOMAN_FRAME_NO_183_SIZE	( 4 )	/* addr + ctrl + frame 183 def */
#define FIOMAN_FRAME_NO_188			( 188 )
#define FIOMAN_FRAME_NO_188_SIZE	( 4 )	/* addr + ctrl + frame 188 def */
#define FIOMAN_FRAME_NO_189			( 189 )
#define FIOMAN_FRAME_NO_189_SIZE	( 180 )	/* addr + ctrl + frame 189 def */
#define FIOMAN_FRAME_NO_190			( 190 )
#define FIOMAN_FRAME_NO_190_SIZE	( 3 )	/* addr + ctrl + frame 190 def */
#define FIOMAN_FRAME_NO_193			( 193 )
#define FIOMAN_FRAME_NO_193_SIZE	( 515 )	/* addr + ctrl + frame 193 def */
#define FIOMAN_FRAME_NO_195			( 195 )
#define FIOMAN_FRAME_NO_195_SIZE	( 26 )	/* addr + ctrl + frame 195 def */




/*  Global section.
-----------------------------------------------------------------------------*/


/*  Public Interface section.
-----------------------------------------------------------------------------*/
void *fioman_ready_generic_tx_frame(FIOMAN_SYS_FIOD*, int,
			unsigned char __user *, unsigned int count);
void *fioman_ready_generic_rx_frame(FIOMAN_SYS_FIOD*, u8);
void *fioman_ready_frame_49( FIOMAN_SYS_FIOD* );
void *fioman_ready_frame_51( FIOMAN_SYS_FIOD* );
void *fioman_ready_frame_52( FIOMAN_SYS_FIOD* );
void *fioman_ready_frame_53( FIOMAN_SYS_FIOD* );
void *fioman_ready_frame_54( FIOMAN_SYS_FIOD* );
void *fioman_ready_frame_55( FIOMAN_SYS_FIOD* );
void *fioman_ready_frame_60( FIOMAN_SYS_FIOD* );
void *fioman_ready_frame_61( FIOMAN_SYS_FIOD* );
void *fioman_ready_frame_62( FIOMAN_SYS_FIOD* );
void *fioman_ready_frame_66( FIOMAN_SYS_FIOD* );
void *fioman_ready_frame_67( FIOMAN_SYS_FIOD* );
void *fioman_ready_frame_9( FIOMAN_SYS_FIOD* );
void *fioman_ready_frame_0( FIOMAN_SYS_FIOD* );
void *fioman_ready_frame_1( FIOMAN_SYS_FIOD* );
void *fioman_ready_frame_3( FIOMAN_SYS_FIOD* );
void *fioman_ready_frame_10_11( FIOMAN_SYS_FIOD*, int );
void *fioman_ready_frame_12_13(	FIOMAN_SYS_FIOD*, int );
void *fioman_ready_frame_18( FIOMAN_SYS_FIOD* );
void *fioman_ready_frame_20_23( FIOMAN_SYS_FIOD*, u8 );
void *fioman_ready_frame_24_27( FIOMAN_SYS_FIOD*, u8 );
void *fioman_ready_frame_138_141( FIOMAN_SYS_FIOD*, u8 );
void *fioman_ready_frame_148_151( FIOMAN_SYS_FIOD*, u8 );
void *fioman_ready_frame_152_155( FIOMAN_SYS_FIOD*, u8 );
void *fioman_ready_frame_177( FIOMAN_SYS_FIOD* );
void *fioman_ready_frame_179( FIOMAN_SYS_FIOD* );
void *fioman_ready_frame_180( FIOMAN_SYS_FIOD* );
void *fioman_ready_frame_181( FIOMAN_SYS_FIOD* );
void *fioman_ready_frame_182( FIOMAN_SYS_FIOD* );
void *fioman_ready_frame_183( FIOMAN_SYS_FIOD* );
void *fioman_ready_frame_188( FIOMAN_SYS_FIOD* );
void *fioman_ready_frame_189( FIOMAN_SYS_FIOD* );
void *fioman_ready_frame_195( FIOMAN_SYS_FIOD* );
void *fioman_ready_frame_128( FIOMAN_SYS_FIOD* );
void *fioman_ready_frame_129( FIOMAN_SYS_FIOD* );
void *fioman_ready_frame_131( FIOMAN_SYS_FIOD* );
void *fioman_ready_frame_190( FIOMAN_SYS_FIOD* );


/* FIOMAN TX update functions */
void fioman_tx_frame_49( FIOMSG_TX_FRAME* );
void fioman_tx_frame_51( FIOMSG_TX_FRAME* );
void fioman_tx_frame_52( FIOMSG_TX_FRAME* );
void fioman_tx_frame_53( FIOMSG_TX_FRAME* );
void fioman_tx_frame_54( FIOMSG_TX_FRAME* );
void fioman_tx_frame_55( FIOMSG_TX_FRAME* );
void fioman_tx_frame_61( FIOMSG_TX_FRAME* );
void fioman_tx_frame_62( FIOMSG_TX_FRAME* );
void fioman_tx_frame_66( FIOMSG_TX_FRAME* );
void fioman_tx_frame_67( FIOMSG_TX_FRAME* );
void fioman_tx_frame_9(	FIOMSG_TX_FRAME* );
void fioman_tx_frame_0(	FIOMSG_TX_FRAME* );
void fioman_tx_frame_10_11( FIOMSG_TX_FRAME* );
void fioman_tx_frame_12_13( FIOMSG_TX_FRAME* );

/* FIOMAN RX update functions */
void fioman_rx_frame_129( FIOMSG_RX_FRAME* );
void fioman_rx_frame_131( FIOMSG_RX_FRAME* );
void fioman_rx_frame_138_141( FIOMSG_RX_FRAME* );
void fioman_rx_frame_148_151( FIOMSG_RX_FRAME* );
void fioman_rx_frame_177( FIOMSG_RX_FRAME* );
void fioman_rx_frame_179( FIOMSG_RX_FRAME* );
void fioman_rx_frame_180( FIOMSG_RX_FRAME* );
void fioman_rx_frame_181( FIOMSG_RX_FRAME* );
void fioman_rx_frame_182( FIOMSG_RX_FRAME* );
void fioman_rx_frame_183( FIOMSG_RX_FRAME* );
void fioman_rx_frame_188( FIOMSG_RX_FRAME* );
void fioman_rx_frame_189( FIOMSG_RX_FRAME* );
void fioman_rx_frame_195( FIOMSG_RX_FRAME* );


#endif /* #ifndef _FIOFRAME_H_ */


/*****************************************************************************/
/*

REVISION HISTORY:
$Log$

*/
/*****************************************************************************/
