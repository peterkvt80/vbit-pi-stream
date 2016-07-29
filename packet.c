/** packet.c
 * Packet handles teletext packets.
 * The basic transmission unit is 45 bytes.
 * These functions are used to set up the various packet types including:
 * header
 * row
 * quiet
 * filler
 * packet 8/30
 * Fastext
 *
 * Copyright (c) 2010-2013 Peter Kwan
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use,
 * copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following
 * conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.

 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 * OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 *
 * The name(s) of the above copyright holders shall not be used in
 * advertising or otherwise to promote the sale, use or other
 * dealings in this Software without prior written authorization.
 *
 *****************************************************************************/ 
#include "packet.h"

double calculateMJD(int year, int month, int day);

void dumpPacket(char* packet)
{
	int i;
	for (i=0;i<45;i++)
		printf("%2x ",packet[i]); 
	printf("\n");
}

/** Copy a line of teletext in MRG tti format.
 * OL,nn,<line> 
 * Where nn is a line number 1..27
 * <line> has two methods of escaping, which need to be decoded
 * \return The row number
 */
uint8_t copyOL(char *packet, char *textline)
{
	int i;
	char* p;
	long linenumber;
	char ch;
	// Get the line number
	textline+=3;
	linenumber=strtol(textline,NULL,10);	
	if (!linenumber) return 0; // Trap OL,0, which I don't think makes any sense but Minited 3xx adds it
	// xatoi(&textline, &linenumber);
	//xprintf(PSTR("Line number=%d\n"),(int)linenumber);
	// Skip to the comma to get the body of the command
	for (i=0;i<4 && ((*textline++)!=',');i++);
	if (*(textline-1)!=',')
	{
		// printf("[copyOL]Fail=%s",textline);
		return 0xff; // failed
	}
	for (p=packet+5;p<(packet+PACKETSIZE);p++)*p=' '; // Stuff with spaces in case the OL command is too short
	for (p=packet+5;*textline && p<(packet+PACKETSIZE);textline++) // Stop on end of file OR packet over run
	{
		// TODO: Also need to check viewdata escapes
		// Only handle MRG mapping atm
		ch=*textline; // Don't strip off the top bit just yet!
		if (ch==0x1b) // De-escape for 7 bit viewdata mode
		{
			textline++;
			ch=((*textline) & 0x1f) | 0x80;	// Turn them into MRG 8 bit escapes
		}
		if ((ch!=0x0d)) // Do not include \r
		{
			*p=ch;
		}
		else
		{
			// \r is not valid in an MRG page, so it must be a truncated line
			// Fill the rest of the line in with blanks
			// Doesn't seem to work on Pi. Maybe only for XMega.
			char *r;
			for (r=p;r<(packet+PACKETSIZE);r++)
				*r=' '; // fill to end with blanks
				// *r='z'-(char)(PACKETSIZE-(char)((int)packet+(int)r)); // was space but I want it visible
			// *r='E';    // Make the last one visibly different (debugging)
			break;
		}
		// if ((*p & 0x7f)==0) *p=' '; // In case a null sneaked in
		p++;
	}
	return linenumber;
} // copyOL

/** Fastext links
 * FL,<link red>,<link green>,<link yellow,<link cyan>,<link>,<link index>
 * \param packet : is the resulting text
 * \param textline : A FL line from a page file
 * \param page : Need this to get the mag. Offsets are relative to the mag
 */
void copyFL(char *packet, char *textline, unsigned char mag)
{
	unsigned long nLink;
	// add the designation code
	char *ptr;
	char *p;
	uint8_t i,j;
	p=packet+5;
	*p++=HamTab[0];	// Designation code 0
	mag&=0x07;		// Mask the mag just in case. Keep it valid
	
	// add the link control byte. This will allow row 24 to show.
	packet[42]=HamTab[0x0f];

	// and the page CRC
	packet[43]=HamTab[0];	// Don't know how to calculate this.
	packet[44]=HamTab[0];

	// for each of the six links
	for (i=0; i<6; i++)
	{
		// TODO: Simplify this. It can't be that difficult to read 6 hex numbers.
		// TODO: It needs to be much more flexible in the formats that it will accept
		// Skip to the comma to get the body of the command
		for (j=0;j<6 && ((*textline++)!=',');j++);
		if (*(textline-1)!=',')
		{
			return; // failed :-(
		}
		// page numbers are hex
		ptr=textline;
		//ptr=textline-2;		
		//*ptr='0';
		//*(ptr+1)='x';
		// xatoi(&ptr,&nLink);
		nLink=strtol(ptr,NULL,16);
		// int nLink =StrToIntDef("0x" + strParams.SubString(1 + i*4,3),0x100);
		if (nLink == 0) nLink = 0x8ff; // turn zero into 8FF to be ignored
		
		// calculate the relative magazine
		char cRelMag=(nLink/0x100 ^ mag);
		*p++=HamTab[nLink & 0xF];			// page units
		*p++=HamTab[(nLink & 0xF0) >> 4];	// page tens
		*p++=HamTab[0xF];									// subcode S1
		*p++=HamTab[((cRelMag & 1) << 3) | 7];
		*p++=HamTab[0xF];
		*p++=HamTab[((cRelMag & 6) << 1) | 3];
		//if (mag==1)
		//{
//			printf("[copyFL]mag 1 link:%X, cRelMag=%d\n",nLink,cRelMag);
		//}
	}	
} // copyFL


/**
 * @brief Check parity and reverse bits if needed
 * \param packet The 45 char row
 * \param Offset is normally 5 for rows, 13 for header
 */
void Parity(char *packet, uint8_t offset)
{
	int i;
	//uint8_t c;
	for (i=offset;i<PACKETSIZE;i++)
	{		
		packet[i]=ParTab[(uint8_t)(packet[i]&0x7f)];
	}
	/* DO NOT REVERSE for Raspi
	for (i=0;i<PACKETSIZE;i++)
	{
		c=(uint8_t)packet[i];
		c = (c & 0x0F) << 4 | (c & 0xF0) >> 4;
		c = (c & 0x33) << 2 | (c & 0xCC) >> 2;
		c = (c & 0x55) << 1 | (c & 0xAA) >> 1;	
		packet[i]=(char)c;
	}
	*/
} // Parity

/**
 * @brief Clear entire packet to a value
 */
void PacketClear(uint8_t *packet, uint8_t value)
{
	uint8_t i;
	for (i=0;i<PACKETSIZE;i++)
		packet[i]=value;
}

// Clear entire packet to 0
void PacketQuiet(uint8_t *packet)
{
	PacketClear(packet,0);
}

// Set CRI and MRAG. Set rest of packet to "value"
void PacketPrefixValue(uint8_t *packet, uint8_t mag, uint8_t row, int value)
{
	char *p=(char*)packet; // Remember that the bit order gets reversed later
	if (value<256) PacketClear(packet,value);	// For debugging this helps
	*p++=0x55; // clock run in
	*p++=0x55; // clock run in
	*p++=0x27; // framing code
	// add MRAG
	*p++=HamTab[mag%8+((row%2)<<3)]; // mag + bit 3 is the lowest bit of row
	*p++=HamTab[((row>>1)&0x0f)];
} // PacketPrefix

// Set CRI and MRAG and ignore everything else
void PacketPrefix(uint8_t *packet, uint8_t mag, uint8_t row)
{
	PacketPrefixValue(packet,mag,row,999); // 999=Don't mess up the rest of the packet!
}

// Make a filler packet
void PacketFiller(uint8_t *packet)
{
	PacketPrefixValue(packet,8,25,' ');
}

/** A header has mag, row=0, page, flags, caption and time
 */
void PacketHeader(char *packet ,unsigned char mag, unsigned char page, unsigned int subcode,
			unsigned int control)
{
	uint8_t cbit;
	PacketPrefix((uint8_t*)packet,mag,0);
	packet[5]=HamTab[page%0x10];
	packet[6]=HamTab[page/0x10];
	packet[7]=HamTab[(subcode&0x0f)]; // S1
	subcode>>=4;
	// Map the page settings control bits from MiniTED to actual teletext packet.
	// To find the MiniTED settings look at the tti format document.
	// To find the target bit position these are in reverse order to tx and not hammed.
	// So for each bit in ETSI document, just divide the bit number by 2 to find the target location.
	// Where ETSI says bit 8,6,4,2 this maps to 4,3,2,1 (where the bits are numbered 1 to 8) 
	cbit=0;
	if (control & 0x4000) cbit=0x08;	// C4 Erase page
	packet[8]=HamTab[(subcode&0x07) | cbit]; // S2 add C4
	subcode>>=3;
	packet[9]=HamTab[(subcode&0x0f)]; // S3
	subcode>>=4;
	cbit=0;
	// Not sure if these bits are reversed. C5 and C6 are indistinguishable
	if (control & 0x0002) cbit=0x08;	// C6 Subtitle
	if (control & 0x0001) cbit|=0x04;	// C5 Newsflash
	
	// cbit|=0x08; // TEMPORARY!
 	packet[10]=HamTab[(subcode&0x03) | cbit]; // S4 C6, C5
	cbit=0;
	if (control & 0x0004)  cbit=0x01;	// C7 Suppress Header TODO: Check if these should be reverse order
	if (control & 0x0008) cbit|=0x02;	// C8 Update
	if (control & 0x0010) cbit|=0x04;	// C9 Interrupted sequence
	if (control & 0x0020) cbit|=0x08;	// C10 Inhibit display
	
	packet[11]=HamTab[cbit]; // C7 to C10
	cbit=(control & 0x0380) >> 6;	// Shift the language bits C12,C13,C14. TODO: Check if C12/C14 need swapping. CHECKED OK.
	if (control & 0x0040) cbit|=0x01;	// C11 serial/parallel
	packet[12]=HamTab[cbit]; // C11 to C14 (C11=0 is parallel, C12,C13,C14 language)
} // Header

void PageEnhancementDataPacket(char *packet, int mag, int row, int designationCode)
{
	PacketPrefixValue((uint8_t*)packet,mag,row,0); // Also clear the remainder to 0
	//packet[5]=HamTab[page%0x10];
	// packet[6]=HamTab[page/0x10];
	packet[5]=HamTab[(designationCode&0x0f)]; // S1
}

// Hamming 24/18
// The incoming triplet should be packed 18 bits of an int 32 representing D1..D18
// The int is repacked with parity bits  
void SetTriplet(char *packet, int ix, int triplet)
{
	uint8_t t[4];
	if (ix<1) return;
	vbi_ham24p(t,triplet);
	// Now stuff the result in the packet
	packet[ix*3+3]=t[0];
	packet[ix*3+4]=t[1];
	packet[ix*3+5]=t[2];
}

// generate packet 8/30
// format must be either 1 or 2
// gets values from global settings in settings.c
void Packet30(uint8_t *packet, uint8_t format)
{
	uint8_t *p;
	uint8_t c;
	time_t timeRaw;
	struct tm * tempTime;
	time_t timeLocal;
	time_t timeUTC;
	int offsetHalfHours;
	int year, month, day, hour, second;
	long modifiedJulianDay;
	
	if (!(format == 1 || format == 2)){
		PacketClear(packet,0); // only format 1 and 2 packets are valid. just output quiet packet
		return;
	}
	
	PacketPrefixValue(packet,8,30,' '); // set the MRAG to 8/30, everything else spaces
	
	p=packet+5;
	
	*p++=HamTab[(format & 0x02) | (multiplexedSignalFlag & 0x01)]; // designation code byte
	
	// initial teletext page, same for both formats
	*p++=HamTab[initialPage & 0xF];                            // page units
	*p++=HamTab[(initialPage & 0xF0) >> 4];                    // page tens
	*p++=HamTab[initialSubcode & 0xF];                         // subcode S1
	initialSubcode>>=4;
	*p++=HamTab[((initialMag & 1) << 3) | (initialSubcode & 7)]; // subcode S2 + M1
	initialSubcode>>=4;
	*p++=HamTab[initialSubcode & 0xF];                         // subcode S3
	initialSubcode>>=4;
	*p++=HamTab[((initialMag & 6) << 1) | (initialSubcode & 3)]; // subcode S4 + M2, M3
	
	if (format == 1){
		// packet is 8/30/0 or 8/30/1
		
		/* NI code has reversed bits some reason */
		c = (NetworkIdentificationCode >> 8) & 0xFF;
		c = (c & 0x0F) << 4 | (c & 0xF0) >> 4;
		c = (c & 0x33) << 2 | (c & 0xCC) >> 2;
		c = (c & 0x55) << 1 | (c & 0xAA) >> 1;
		*p++=c;
		c = NetworkIdentificationCode & 0xFF;
		c = (c & 0x0F) << 4 | (c & 0xF0) >> 4;
		c = (c & 0x33) << 2 | (c & 0xCC) >> 2;
		c = (c & 0x55) << 1 | (c & 0xAA) >> 1;
		*p++=c;
		
		/* calculate number of seconds local time is offset from UTC */
		timeRaw = time(NULL);
		timeLocal = mktime(localtime(&timeRaw));
		timeUTC = mktime(gmtime(&timeRaw));
		offsetHalfHours = difftime(timeLocal, timeUTC) / 1800;
		//fprintf(stderr,"Difference in half hours: %d\n", offsetHalfHours);
		
		// time offset code -bits 2-6 half hours offset from UTC, bit 7 sign bit
		// bits 0 and 7 reserved - set to 1
		*p++ = ((offsetHalfHours < 0) ? 0xC1 : 0x81) | ((abs(offsetHalfHours) & 0x1F) << 1);
		
		// get the time current UTC time into separate variables
		tempTime = gmtime(&timeRaw);
		year = tempTime->tm_year + 1900;
		month = tempTime->tm_mon + 1;
		day = tempTime->tm_mday;
		hour = tempTime->tm_hour;
		second = tempTime->tm_sec;
		
		//fprintf(stderr,"y %d, m %d, d %d\n", year, month, day);
		modifiedJulianDay = calculateMJD(year, month, day);
		fprintf(stderr,"Modified Julian day: %ld\n", modifiedJulianDay);
		fprintf(stderr,"%ld %ld %ld %ld %ld\n", (modifiedJulianDay % 100000 / 10000 + 1),(modifiedJulianDay % 10000 / 1000 + 1),(modifiedJulianDay % 1000 / 100 + 1),(modifiedJulianDay % 100 / 10 + 1),(modifiedJulianDay % 10 + 1));
		// generate five decimal digits of modified julian date decimal digits and increment each one.
		*p++ = (modifiedJulianDay % 100000 / 10000 + 1);
		*p++ = ((modifiedJulianDay % 10000 / 1000 + 1) << 4) | (modifiedJulianDay % 1000 / 100 + 1);
		*p++ = ((modifiedJulianDay % 100 / 10 + 1) << 4) | (modifiedJulianDay % 10 + 1);
		
		
		
	} else {
		// packet must be 8/30/2 or 8/30/3
		
		
	}
	
	memcpy(packet+25, "VBIT", 4); // temporarily stick in something for the status
	
	return;
}

double calculateMJD(int year, int month, int day){
	// calculate modified julian day number
	int a, m, y;
	a = (14 - month) / 12;
	y = year + 4800 - a;
	m = month + (12 * a) - 3;
	return day + ((153 * m + 1)/5) + (365 * y) + (y / 4) - (y / 100) + (y / 400) - 32045 - 2400000.5;
}
