/** ***************************************************************************
 * Title			: buffer.c
 * Description       : VBIT packet buffering
 * Initialise and manage circular buffers:
 * Initialise buffer
 * Add packet to buffer
 * Pop packet from buffer
 * Buffer tests: full, empty
 * It also detects and rewrites teletext header packets.
 * Support multiple buffers: one per magazine and one for stream.c
 * 
 * Compiler          : GCC
 *
 * Copyright (C) 2013-2015, Peter Kwan
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
 ****************************************************************************/
 
//#define _DEBUG_

#include "buffer.h"

// Byte reverser
// Either Define this for VBIT
// or Comment it out for raspi-teletext
// #define REVERSE

// Could do with a buffer structure
// An array of packets
// Buffer structure: pointer to first packet, last packet head, tail.

/**bufferInit
 * Sets up a packet buffer
 * \param bp - A bufferpacket control block
 * \param buf - The address of the packet buffer
 * \param len - The number of packets in the buffer
 */
void bufferInit(bufferpacket *bp, char *buf, uint8_t len)
{
#ifdef _DEBUG_
fprintf(stderr,"[bufferInit] packets=%d\n",len);
#endif 
	bp->count=len;
	bp->pkt=buf;
	bp->head=0;
	bp->tail=0;	
}

/**bufferPut
 * \brief Put packet pkt onto bufferpacket bp.
 * \param pkt : Packet to put
 * \param bp : buffer to put the packet onto
 * \return BUFFER_OK if OK BUFFER_FULL if full.
 */
uint8_t bufferPut(bufferpacket *bp, char *pkt)
{
	int i;
	char *p;
	char *q;
	if (bufferIsFull(bp)) return BUFFER_FULL;
	p=bp->pkt+bp->head*PACKETSIZE;	// Destination
	q=pkt;							// Source
	// Copy the packet
	for (i=0;i<PACKETSIZE;i++)
		*p++=*q++;
	// Update the head pointer	
	bp->head=(bp->head+1) % bp->count;
	return BUFFER_OK;
}

/**bufferGet
 * Get packet pkt from bufferpacket bp.
 * \param pkt : Packet to accept pop
 * \param bp : buffer to pop packet from
 * \return BUFFER_OK if OK BUFFER_EMPTY if empty.
 */
uint8_t bufferGet(bufferpacket *bp, char *pkt)
{
	int i;
	char *p;
	char *q;
	if (bufferIsEmpty(bp)) return BUFFER_EMPTY;	// Nothing to get? Return BUFFER_EMPTY (1)
	p=bp->pkt+bp->tail*PACKETSIZE;	// Source
	q=pkt;							// Destination
	// Fetch the packet
	for (i=0;i<PACKETSIZE;i++)
		*q++=*p++;
	// Step the tail pointer	
	bp->tail=(bp->tail+1) % bp->count;
	return BUFFER_OK;
}

/**bufferIsEmpty
 * Test whether a buffer is empty
 * \param bp : buffer to test
 * \return BUFFER_OK if not empty BUFFER_EMPTY if empty.
 */
uint8_t bufferIsEmpty(bufferpacket *bp)
{
	if (bp->tail==bp->head) return BUFFER_EMPTY;		// head and tail are the same?
	return BUFFER_OK;
}

/**bufferIsFull
 * Test whether a buffer is full
 * \param bp : buffer to test
 * \return BUFFER_OK if OK or BUFFER_FULL if full
 */
uint8_t bufferIsFull(bufferpacket *bp)
{
	if (((bp->head+1) % bp->count) == bp->tail) return BUFFER_FULL; // Incrementing the head would hit the tail?
	return BUFFER_OK;
}

// What is this for? It will let stream work out what the next line is
// and if it is on the next field.
//... but this seems too complicated, Must be an easier way to maintain the line count
uint8_t bufferLevel(bufferpacket *bp)
{
	if (bp->head>=bp->tail)
		return (bp->head-bp->tail);
	else
		return (bp->count-bp->head+bp->tail);
}

 /** buffermove
  * Pops from buffer src and pushes to dest
  * This is used to multiplex mag to stream.
  * \param dest Destination buffer
  * \param src Source buffer
  * \return BUFFER_OK=OK, BUFFER_HEADER=header, BUFFER_FULL=destination full, BUFFER_EMPTY=source empty
  * We do some stupid stuff: decoding packets that we only coded a fraction of a second ago
  * mainly because we don't pass any other data between threads.
  */
uint8_t bufferMove(bufferpacket *dest, bufferpacket *src)
{
	char a,b; /// Temporary stores to help reverse bytes if the transmission hardware requires it (Vbit-Pi inserter needs reverse. VBIT-Pi-Stream does not.)
	uint8_t row;
	uint8_t returnCode=BUFFER_OK;
	char pkt[PACKETSIZE];
	uint8_t i;
	// uint8_t c;
	time_t timer;
	char str[9];
	struct tm * timeinfo;
	char* ptr;
	char * ptr2;
	uint8_t mag;
	
	if (bufferIsFull(dest))	// Quit if destination is full
		return BUFFER_FULL;
	if (bufferGet(src,pkt)==BUFFER_EMPTY)	// Quit if source is empty
		return BUFFER_EMPTY;
	
	// Test if MRAG is an header
	// So decode the packet.
	// reverse the bit order
	a =(uint8_t)pkt[3];
#ifdef REVERSE	
	a = (a & 0x0F) << 4 | (a & 0xF0) >> 4;
	a = (a & 0x33) << 2 | (a & 0xCC) >> 2;
	a = (a & 0x55) << 1 | (a & 0xAA) >> 1;	
#endif
	// mask the parity
	a &= 0x7f;
	// and deham the result
	a=DehamTable[(uint8_t)a];
	mag=a;
	if (mag==0) mag=8;
	
	// And again for the next byte
	b =(uint8_t)pkt[4];
#ifdef REVERSE
	b = (b & 0x0F) << 4 | (b & 0xF0) >> 4;
	b = (b & 0x33) << 2 | (b & 0xCC) >> 2;
	b = (b & 0x55) << 1 | (b & 0xAA) >> 1;	
#endif
	// mask the parity
	b &= 0x7f;
	// and deham the result
	b=DehamTable[(uint8_t)b];
	row=a & 0x08;
	row+=b;
	
	// If the row is 0, return the fact that it is an header
	// In addition put in all the dynamic elements
	if (!row)	// Format the header here
	{
		// What is the header?
		// Four blanks, three page digits, another blank, and 32 chars of data. The last 8 digits are for the clock but this seems to be convention.
		// Insert page number, date, clock
		// "mpp MRG DAY dd MTH"
		// HERE WE TRANSLATE ALL THE HEADER BITS
		
		/* TODO: format the template
		p=strstr(packet,"mpp"); 
		if (p) // if we have mpp, replace it with the actual page number...
		{
			*p++=mag+'0';
			ch=page>>4; // page tens (note wacky way of converting digit to hex)
			*p++=ch+(ch>9?'7':'0');
			ch=page%0x10; // page units
			*p++=ch+(ch>9?'7':'0');
		}		
*/		
		// What are we going to write? The last 32 bytes of the header. The first 8 bytes are for control flags and stuff.
		// Fill the buffer with dummy data. Trust me. It will help with debugging.
		ptr=&pkt[PACKETSIZE-32];
		ptr2=headerTemplate;
		
		for (i=0;i<32;i++)
			// *ptr++=i+'0';	// Copy pattern	
			*ptr++=*ptr2++;		// Copy the built in template
	
		ptr=&pkt[PACKETSIZE-32];	// Reset the packet pointer
		// Do substitutions. Only allow each one once per header
		// MPP - Magazine and page number
		
		ptr2=strstr(ptr,"%%#");
		
		if (ptr2)
		{
		
			//ptr2[0]=(a & 0x07)+'1';	// Mag
			ptr2[0]=(mag & 0x0f)+'0';	// Mag
			// ptr2[3]=((mag & 0xf0)>>4) + 'a'; // temp
			a =(uint8_t)pkt[6];
#ifdef REVERSE			
			a = (a & 0x0F) << 4 | (a & 0xF0) >> 4;
			a = (a & 0x33) << 2 | (a & 0xCC) >> 2;
			a = (a & 0x55) << 1 | (a & 0xAA) >> 1;	
#endif
			// mask the parity
			a &= 0x7f;			
			ptr2[1]=(DehamTable[(uint8_t)a]&0x0f)+'0'; 	// Page (ten)
			if (ptr2[1]>'9')
				ptr2[1]=ptr2[1]-'0'-10+'A'; 	// Particularly poor hex conversion algorithm

			a =(uint8_t)pkt[5];
#ifdef REVERSE
			a = (a & 0x0F) << 4 | (a & 0xF0) >> 4;
			a = (a & 0x33) << 2 | (a & 0xCC) >> 2;
			a = (a & 0x55) << 1 | (a & 0xAA) >> 1;	
#endif			
			// mask the parity
			a &= 0x7f;			
			ptr2[2]=(DehamTable[(uint8_t)a]&0x0f)+'0';	// Page (unit)
			if (ptr2[2]>'9')
				ptr2[2]=ptr2[2]-'0'-10+'A'; 	// Particularly poor hex conversion algorithm
			
			// TEST
			b =(uint8_t)pkt[3];
#ifdef REVERSE
			b = (b & 0x0F) << 4 | (b & 0xF0) >> 4;
			b = (b & 0x33) << 2 | (b & 0xCC) >> 2;
			b = (b & 0x55) << 1 | (b & 0xAA) >> 1;	
#endif			
			b&=0x7f;
			// c=DehamTable[(uint8_t) b];
			// printf("ptr[3]=%02x Rev=%02x mag=%02x mag=%02x. ",pkt[3],b,c,mag);
		}
		
		timer=time(NULL);
		timeinfo=localtime(&timer);	// This gets local time.

		ptr2=strstr(ptr,"%%a");	// Tue
		if (ptr2)
		{
			strftime(str,10,"%a",timeinfo);
			ptr2[0]=str[0];
			ptr2[1]=str[1];
			ptr2[2]=str[2];
		}
			
		ptr2=strstr(ptr,"%%b"); // Jan
		if (ptr2)
		{
			strftime(str,10,"%b",timeinfo);
			ptr2[0]=str[0];
			ptr2[1]=str[1];
			ptr2[2]=str[2];
		}
		
		ptr2=strstr(ptr,"%d");	// day of month with leading zero
		if (ptr2)
		{
			strftime(str,10,"%d",timeinfo);
			ptr2[0]=str[0];
			ptr2[1]=str[1];
		}
		
		ptr2=strstr(ptr,"%e");	// day of month with no leading zero
		if (ptr2)
		{
			#ifndef WIN32
			strftime(str,10,"%e",timeinfo);
			ptr2[0]=str[0];
			#else
			strftime(str,10,"%d",timeinfo); // mingw doesn't know %e
			ptr2[0]=' ';
			#endif
			ptr2[1]=str[1];
		}		

		ptr2=strstr(ptr,"%m");	// month number with leading 0
		if (ptr2)
		{
			strftime(str,10,"%m",timeinfo);
			ptr2[0]=str[0];
			ptr2[1]=str[1];
		}		

		ptr2=strstr(ptr,"%g");	// year. 2 digits
		if (ptr2)
		{
			strftime(str,10,"%g",timeinfo); // TODO: mingw support
			ptr2[0]=str[0];
			ptr2[1]=str[1];
		}
		
		ptr2=strstr(ptr,"%H");	// hour.
		if (ptr2)
		{
			strftime(str,10,"%H",timeinfo);
			ptr2[0]=str[0];
			ptr2[1]=str[1];
		}
		
		ptr2=strstr(ptr,"%M");	// minutes.
		if (ptr2)
		{
			strftime(str,10,"%M",timeinfo);
			ptr2[0]=str[0];
			ptr2[1]=str[1];
		}
		
		ptr2=strstr(ptr,"%S");	// seconds.
		if (ptr2)
		{
			strftime(str,10,"%S",timeinfo);
			ptr2[0]=str[0];
			ptr2[1]=str[1];
		}

		//strftime(str,9,"%H:%M/%S",timeinfo); // TODO: Use the template
		//printf("The current time is %s.\n",str);
		// This code below is the old clock stuff (locked to video)
		//strncpy(&pkt[37],str,8);
		// sprintf(&pkt[37],"%02d:%02d:%02d",hours,mins,secs);
		// Parity(pkt,30);  <-- We need parity, but this kills it!
		// Slightly changed version of Parity()
		//pkt[36]=0x83; // Yellow text

		for (i=PACKETSIZE-32;i<PACKETSIZE;i++)
		{			
			pkt[i]=ParTab[(uint8_t)(pkt[i]&0x7f)]; 
#ifdef REVERSE
			c=(uint8_t)pkt[i];
			c = (c & 0x0F) << 4 | (c & 0xF0) >> 4;
			c = (c & 0x33) << 2 | (c & 0xCC) >> 2;
			c = (c & 0x55) << 1 | (c & 0xAA) >> 1;	
			pkt[i]=(char)c;
#endif			
		}
		
		returnCode=BUFFER_HEADER;	// Signal that this is a mag header
	}	
	
	// Finally send this buffer to the output stream
	bufferPut(dest,pkt);

	return returnCode;
}
  
