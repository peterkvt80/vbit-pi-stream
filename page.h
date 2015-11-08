/** ***************************************************************************
 * Description       : VBIT Teletext Page Parser
 * Compiler          : GCC
 *
 * Copyright (C) 2010-2012, Peter Kwan
 *
 * Permission to use, copy, modify, and distribute this software
 * and its documentation for any purpose and without fee is hereby
 * granted, provided that the above copyright notice appear in all
 * copies and that both that the copyright notice and this
 * permission notice and warranty disclaimer appear in supporting
 * documentation, and that the name of the author not be used in
 * advertising or publicity pertaining to distribution of the
 * software without specific, written prior permission.
 *
 * The author disclaims all warranties with regard to this
 * software, including all implied warranties of merchantability
 * and fitness.  In no event shall the author be liable for any
 * special, indirect or consequential damages or any damages
 * whatsoever resulting from loss of use, data or profits, whether
 * in an action of contract, negligence or other tortious action,
 * arising out of or in connection with the use or performance of
 * this software.
 *************************************************************************** **/
#ifndef _PAGE_H_
#define _PAGE_H_

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>     /* atoi */

/** Structure and routines parse and store page details
 */
typedef struct _PAGE_ 
{
	char filename[80]; /// May need to cut this down if we get short of RAM
	unsigned char mag;		/// 1..8 magazine number
	unsigned char page;		/// 00..99 page number
	unsigned char subpage;	/// 00..99 (not part of ETSI spec [or is it?])
	unsigned int subcode;	/// subcode (we use it to hold subpage)
	unsigned char timerMode; /// C=cycle. Counts the retransmits before the next page. T=timed. C seems like a daft idea to me so we will default to T.
	unsigned int time;		/// seconds (from CT command)
	unsigned int control;	/// C bits and non ETSI bits (See tti specification)
	unsigned int filesize;	/// Size (bytes) of the file that this page was parsed from
	unsigned int redirect;	/// FIFO ram page to get text data from, instead of from the file. 0..SRAMPAGECOUNT
	unsigned int region;	/// Region selects a codepage set. 0,1,2,3,4,6,8,10
} PAGE;

/** Parse a single line of a tti teletext page
 * \param str - String to parse
 * \param page - page to return values in
 * \return true if there is an error
 */
uint8_t ParseLine(PAGE *page, char *str);

/** Parse a teletext page
 * \param filaname - Name of the teletext file
 * \return true if there is an error
 */
uint8_t ParsePage(PAGE *page, char *filename);

/** Clear a page structure
 * The mag is set to 0x99 as a signal that it is not valid
 * Other variables are set null or invalid
 * \param page - Pointer to PAGE structure
 */
void ClearPage(PAGE *page);

#endif
