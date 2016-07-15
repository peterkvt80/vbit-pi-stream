/** ***************************************************************************
 * Name				: page.c
 * Description       : VBIT Teletext Page Parser
 *
 * Copyright (C) 2010-2013, Peter Kwan
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
#include "page.h"

/** Need to make this more like linux
static void put_rc (FRESULT rc)
{
	const prog_char *p;
	static const prog_char str[] =
		"OK\0" "DISK_ERR\0" "INT_ERR\0" "NOT_READY\0" "NO_FILE\0" "NO_PATH\0"
		"INVALID_NAME\0" "DENIED\0" "EXIST\0" "INVALID_OBJECT\0" "WRITE_PROTECTED\0"
		"INVALID_DRIVE\0" "NOT_ENABLED\0" "NO_FILE_SYSTEM\0" "MKFS_ABORTED\0" "TIMEOUT\0";
	FRESULT i;
	if (rc==0) return; // Not interested in OK
	for (p = str, i = 0; i != rc && pgm_read_byte_near(p); i++) {
		while(pgm_read_byte_near(p++));
	}
	xprintf(PSTR("[page.c]rc=%u FR_%S\n"), (WORD)rc, p);	// TODO
}
*/

/** Parse a single line of a tti teletext page
 * Each entry in a tti file must start two letters and a comma
 * \param str - String to parse
 * \param page - page to return values in
 * \return 1 if there is an error, otherwise 0
 */
uint8_t ParseLine(PAGE *page, char *str)
{
	int32_t n;
	// TODO: More input checking
//	printf("[ParseLine] Parsing %s\n",str);
	if (str[0]==0) return 0;	// Nothing to parse
	if (str[2]!=',')
	{
		// Not sure why lines with too many spaces break this. A junk line will do this.
		// printf("[Parse line]Bad format %s\n",str); // Comma in wrong place
		return 1;
	}
	switch (str[0])
	{
	case 'D':; // DE - description, DT - date or DS - ?
		//printf ("[ParseLine] Parsing description %s\n",str);
		break;			// This is not implemented - no memory on xmega:-(
	case 'P': 	// PN or PS
		if (str[1]=='N') // PN is 3 or 5 hex digits
		{
			page->page=0x98; // Remove these two lines, they are only for testing
			page->mag=0x99;
			str[1]='0';
			str[2]='x';
			n=strtol(&str[1],NULL,0);
			// printf ("[ParseLine] Parsing page %s\n",&str[1]);
			if (n>0x8ff)
			{
				page->subpage=n%0x100;
				n/=0x100;
			}
			else
				page->subpage=0;
			page->mag=n/0x100;
			page->page=n%0x100;	
			if (page->mag>8)
			{
				// printf("error in line %s\n",&str[1]);
				return 1;
			}
			// printf("[ParseLine]PN mag=%d page=%X, subpage=%X\n",page->mag,page->page,page->subpage);
		}
		else
		if (str[1]=='S')
		{
			str[1]='0';
			str[2]='x';
			n=strtol(&str[1],NULL,0);
			n|=0x8000;	// Add the transmission flag. Why wouldn't you want to transmit?
			n&=~0x0040;	// Remove the serial flag, VBIT is parallel only
			page->control=n;
		}
		break;
	case 'C': // CT,nn,<T|C> - cycle time
		page->timerMode='T';	// C is not implemented and was a daft idea in any case
		// get the nn which we expect to start at str[3]
		page->time=strtol(&str[3],NULL,0);
		break;
	case 'S': // SP - filename or SC - subcode
		// Subcode is HEX! 0000..3F7F
		if (str[1]=='C')
		{
			n=strtol(&str[3],NULL,16);
			// if (n>0) printf("Lets decode a subcode. %04x YAY!\n",n);
			// n=0;	// TODO: Set this to 0 FOR NOW. It will kill carousels
			page->subcode=n;	// When subcode is greater than 0 it is a carousel
		}
		break;
	//Why don't we decode these entries?
	// 1) Either we don't need them, or we do need them later but historically, we needed to save AVR memory 
	// Don't need to save memory on Raspberry Pi.
	case 'M':; // MS - Minited mask (we don't implement this)
		break;
	case 'O':; // OL - output line
		break;
	case 'F':; // FL - fastext links
		break;
	case 'R':; // RT, RD or RE
		if (str[1]=='T') // RT - readback time isn't relevant
			break;			
		if (str[1]=='D') // RD,<n> - redirect. Read the data lines from the FIFO rather than the page file OL commands.
		{
			// The idea is that we can use a reserved area of RAM for dynamic pages.
			// These are pages that change a lot and don't suit being stored in SD card
			// 1) Read the RD parameter, which is a number between 0 and SRAMPAGECOUNT (actually 0x0e (14) atm)
			// We will store this in the page structure ready for the packetizer to grab the SRAM 
			str[1]='0';
			str[2]='x';
			n=strtol(&str[1],NULL,0);
			page->redirect=n;
			break;			
		}
		if (str[1]=='E') // RE - REgion. The number is put into X/28/0
		{
			page->region=strtol(&str[3],NULL,0);
			// printf("Got a region code %d\n",page->region);
			break;			
		}
		
	default :
		// printf("[Parse page]unhandled page code=%c\n",str[0]);	
		return 1;
	}
	return 0;
} // ParseLine

/** Parse a teletext page
 * \param filename - Name of the teletext file
 * \return true if there is an error
 */
uint8_t ParsePage(PAGE *page, char *filename)
{
	FILE *file;
	char *str;
	const unsigned char MAXLINE=200;
	char line[MAXLINE];
	// printf("[Parse page]Started looking at %s\n",filename);
	// open the page
	file=fopen(filename,"rb");
	if (!file)
	{
		// printf("[Parse page]Failed to open tti page\n");			
		//put_rc(res);
		return 1;
	}
	// Shouldn't we clear the page at this point?
	ClearPage(page);
	// page->filesize=(unsigned int)file.fsize; // Not sure that Pi needs this
	// Read a line
	// printf("[ParsePage]Ready to parse\n");
	// If the file has a UTF-8 header, we should get rid of it. 
	int ch;
	ch=getc(file);
	if (ch==0xEF)
	{
		ch=getc(file); // 0xBB
		ch=getc(file); // 0xBF
	}
	else
		rewind(file);
	while (!feof(file))
	{
		str=fgets(line,MAXLINE,file);
		if (!str) break;	// Ooops, no more data
		/* debug
		for (i=0;i<16;i++)
			printf("%02x ",str[i]);
		printf("[ParsePage]Parsing a line %s\n",str);
		printf("\n");
		*/
		if (ParseLine(page,str))
		{
			fclose(file);
			return 1;
		}
		// printf("[ParsePage] chewing next line\n");
	}
	// printf("[ParsePage] mag=%d page=%X, subpage=%X\n",page->mag,page->page,page->subpage);
	// delay(1000);

	fclose(file);
	// printf("[Parse page]Ended\n");	
	
	return 0;
}

/** ClearPage initialises all the variables in a page structure
 * \param page A pointer to a page structure
 */
void ClearPage(PAGE *page)
{
	page->filename[0]=0;
	page->mag=0x99;
	page->page=0xff;
	page->subpage=0xff;
	page->timerMode='T';
	page->time=0;
	page->control=0x8000;	// Default to parallel transmission
	page->filesize=0;
	page->redirect=0xff;	// Which SRAM page to redirect input from. 0..14 or 0xff for None (Not used on VBIT-Pi)
	page->subcode=0; 
	page->region=0;			// region (codebase select for extra languages)
} // ClearPage
