/** ***************************************************************************
 * Description       : VBIT: Magazine page to packet converter.
 * The Pages folder is scanned for pages belonging to a mag.
 * This list of pages is used to sequence packets for this mag.
 * There are eight instances of this thread, one per mag.
 *
 * The main problem with this code is that it doesn't manage 
 * transmission rules properly and decoders might miss a line or two
 * if the header goes out on the same field.
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
/** Mag thread
 * Each magazine operates in its own thread.
 * Each mag knows what mag number it is.
 * Each mag has a state. IDLE/HEADER/ROW to help sequence packets
 * Each mag buffers about 50 packets ahead.
 */

#include "mag.h"
// Number of packets in a magazine buffer. 17 is an arbitrary number


#define PACKETCOUNT 17

// MAXCAROUSEL is an arbitrary number, the maximum number of carousels per magazine 
// 16 is a good value. Should not have too many carousels as it slows the main service.
// but increase this if you need more.
#define MAXCAROUSEL 16

int r1=0;	// Not actually used, just a dummy arg.

// uint8_t thismag;

bufferpacket magBuffer[9];	// One buffer control block for each magazine (plus 1 for out-of-sequence packets like subtitles)


uint8_t magPacket[9][PACKETCOUNT][PACKETSIZE];	// 9 threads, 17 packets, 45 bytes per packet  

static pthread_t magThread[8];

static uint8_t magCount=1;	// Ensure that each thread has a different mag number

// Carousel stuff
typedef struct _CAROUSEL_ 
{
	PAGE *page;		/// Page meta data 
	time_t time;	/// System time of the next transmission 
	uint32_t subcode;	/// Single pages tend to set this 0. Carousels start with 1
} CAROUSEL;

/** addCarousel
 * Adds a carousel page p to the carousel list c.
 * \param c : A carousel list
 * \param p : A page meta data object
 * \return 0 OK, 1 Fail
 */
uint8_t addCarousel(CAROUSEL *c,PAGE *p)
{
	uint8_t i,found,foundindex;	// found an empty cell
	found=0;
	foundindex=0;
	for (i=0;i<MAXCAROUSEL;i++)
	{
		// Not sure why this doesn't work, or if we actually need it
		if (c[i].page==p)
		{
			// printf("[addCarousel]Oh dear, we found a duplicate. Now what? %ul ",(unsigned int)p); // Ignore it and hope it goes away
			return 1;
		}
		if (c[i].page==NULL && !found) // record the first empty slot found
		{
			found=1;
			foundindex=i;
			// printf("[addCarousel] Found = %d\n",foundindex);
		}
	}
	if (!found)	// No available slots left
	{
		// printf("[addCarousel]Can't add carousel. Sorry\n"); // oh no
		return 1;
	}

	// We found an empty slot. Lets go fill it
	// printf("[addCarousel]Carousel \"%s\" (%d%02x), SC=%d in %d\n",p->filename,p->mag,p->page,p->subcode,foundindex);
	// printf("[addCarousel] p=%u\n",(unsigned int)p);
	c[foundindex].page=p;
	c[foundindex].subcode=0;	// Start from a sensible place
	c[foundindex].time=time(NULL);
	return 0;	
}

/** pageToTransmit - Find the next carousel page update
 * \param c : The carousels array
 * \param fp : A file pointer. If there is a carousel, the file pointer will be set to the required page
 * \param page : The page object for the selected page
 * \return Time when the carousel page changes. Or if 0, there is no page.
 */
 time_t pageToTransmit(CAROUSEL *c, FILE** fp, PAGE *page,uint8_t mag)
{
	const uint16_t MAXLINE=200;
	time_t t;
	time_t retval=0;
	uint8_t i;
	uint8_t timeInterval=0;
	uint32_t sc;
	char str[MAXLINE]; // Make this longer than the longest possible description line, because if it is bigger it will mute the page.
	//printf("K");
	PAGE p;

	if (!c[0].page) return 0;	// If there are no carousels, exit immediately
	ClearPage(&p);
	//sprintf(p.filename,"no filename found on mag %d",mag);
	// Get the current time
	t=time(NULL);
	// Which page is ready to go?
	for (i=0;i<MAXCAROUSEL;i++)	// Check all the carousels
	{
		if ((c[i].page!=NULL) && c[i].time<t)	// Is the page due?
		{
			// This is all about finding the next subcode and 
			// moving the file pointer ready to transmit the next page
			//printf("Seeking subcode %d \n",c[i].subcode);
			//printf("Opening %s \n",c[i].page->filename);
			strcpy(p.filename,c[i].page->filename);
			*fp=fopen(c[i].page->filename,"r");
			if (!*fp)
			{
				// printf("[pageToTransmit] file open Failed: Placeholder SEVEN str=%s\n",str);
				*fp=NULL;
				return 0;
			}

			// Parse until we find a bigger subpage
			sc=c[i].subcode;	// The existing subcode
			while (p.subcode<=sc && !feof(*fp))	// Parse the carousel
			{
				fgets(str,MAXLINE,*fp);
				// printf("[pageToTransmit]Parsing %s",str);	// Note: The line already has \n in it
				if (ParseLine(&p, str))					// The parse failed
				{
					// printf("[pageToTransmit] Parse Failed: Placeholder FOUR str=%s fn=%s\n",str,c[i].page->filename);
					delay(1000);
				}
				// Note: A carousel takes timing from the first CT command only
				if (p.time && !timeInterval)
				{
					timeInterval=p.time;
					// printf("[pageToTransmit] Mag %d, time interval=%d\n",mag, p.time);
				}
			}
			// timeInterval=p.time;
			// At this point we either have the next page or we ran off the end
			// printf("[pageToTransmit] page %d %d Entered with %d, next found %d\n",p.mag,p.page,sc,p.subcode);
			// If we hit the end of file, we should restart with subcode 1
	//printf("T\n");
			if (feof(*fp)) // Ran off the end
			{
	//printf("X\n");
				// Loop back to the start of the carousel
				// Just reposition the text pointer to the start
				if (fseek(*fp,0,0))
				{
					// printf("[pageToTransmit] Seek failed: Placeholder FIVE str=%s\n",str);
				}
				// And set the subcode to 1 (first page of a carousel) 
				c[i].subcode=0; // The next subcode to expect would be 1, but 0 should work too.
				// printf("[pageToTransmit] Out of data. Placeholder ONE\n");
			}
			else
			{
					//printf("Y\n");
// TODO: This works if SC follows CT otherwise we will have to do a bit more parsing
				c[i].subcode=p.subcode;
				if (p.time>0)
					timeInterval=p.time;	// Good
				else
				{
					timeInterval=15;		// Error, set sensible default
					//printf("[pageToTransmit] Missing time interval. Placeholder SIX\n");
				}
					//printf("Z\n");

			}
			// Reschedule this carousel
			c[i].time=time(NULL)+timeInterval; 
			break;
		} // page is due
	} // for
	// Now work out when the next carousel page changes.
	retval=0;
	for (i=0;i<MAXCAROUSEL;i++)
	{
		if (c[i].page) // if the page exists
		{
			if (c[i].time>0)	// a valid time 
			{
				// Save the time if we don't have one or it is less than what we have
				if (c[i].time<retval || retval==0)
				{
					retval=c[i].time;
				}
			}
			else
			{
				// printf("[pageToTransmit] Mag %d, carousel %d, Time is 0. This is bad\n",mag,i);
			}
		}
	}
	// printf("Mag %d, Next retval=%d\n",mag,(int)retval);
	if (!p.filename[0])	// For some reason got here without a page? (If ClearPage sets a null string)
	{
		// printf("[pageToTransmit] filename bug that needs fixing\n");
		retval=0;	// Really need to investigate how this fails. It should never happen.
	}
	
	*page=p;
	// printf("[PageToTransmit] L filename=%s mag=%d page=%02x, subcode=%d\n",p.filename,p.mag,p.page,p.subcode);
	// printf("M retval=%d\n",(int)retval);
	return retval;	
} // pageToTransmit

/** getMag - Allocate a magazine to a thread.
 */
uint8_t getMag(void)
{
	uint8_t num;	
	piLock(0);	// Ensure that each mag is unique
	num=magCount;
	magCount=(magCount+1)%8;
	piUnlock(0);
	return num;
}

/** getList - Populate a magazine list
 * Declare the list in domag so we use auto variables. So each thread gets its own environment.
 */
uint8_t getList(PAGE **txList,uint8_t mag, CAROUSEL *carousel)
{
	DIR *d;		// Directory handle
	PAGE page;
	PAGE *p;		// Page that we are going to parse
	PAGE *newpage;
	char path[132];
	char filename[132];
	struct dirent *dir;
	uint8_t i;
	// Make sure all the carousel entries are NULL
	for (i=0;i<MAXCAROUSEL;i++)
	{
		carousel[i].page=NULL;
		carousel[i].time=0;
		carousel[i].subcode=0;
	}
  strcpy(path,"/home/pi/Pages/");	// TODO: Maybe we should use ~/Pages instead?
  //printf("Looking for pages in stream %d\n",mag);
  d = opendir(path);
  p=&page;
  if (d)
  {
    while ((dir = readdir(d)) != NULL)
    {
	  // TODO: Is it a directory?
	  // Is it a tti page
	  if (strcasestr(dir->d_name,".tti") || strcasestr(dir->d_name,".ttix"))
	  {
		  strcpy(filename,path);
		  strcat(filename, dir->d_name);
		  //printf("stream %d, %s\n", mag, filename);
		  if (ParsePage(p, filename))
		  {
				//printf("Not a valid page %s\n",filename);
		  }
		  //printf("Comparing p->mag %d, p->page %d, mag %d\n",p->mag % 8, p->page, mag);
		  if ((p->mag % 8)==mag)
		  {
			strcpy(p->filename,filename);
			//printf("Accepted mag %d page %s\n",mag, p->filename);
			// Create a new page object
			newpage=calloc(1,sizeof(PAGE));
			// Copy the data
			*newpage=*p;
			// And drop the pointer into the transmission list
			// Check that we don't have a duplicate!!!
			if (txList[p->page])
			{
				//printf("[mag:getList] Page already exists. old=%s, new=%s\n",txList[p->page]->filename,p->filename);
			}
			// If subcode is greater than 1 we want to save that page as a carousel
			if (p->subcode>1)
			{
				// printf("subcode=%d ",p->subcode);
				addCarousel(carousel,newpage);
			}
			 else 
				txList[p->page]=newpage;	// Store as a normal non carouselling page
			//printf("[getList]Saved page %s mpp=%01d%02d\n",txList[p->page]->filename,txList[p->page]->mag,txList[p->page]->subpage);
		  }
	  }
	  // TODO: Something wonderful with the PAGE object
    }

    closedir(d);
  }
  else
	return 1;

  return 0;	
} // getList

/** domag is the thread that manages a single magazine
 * Finds and sorts all the pages for a particular magazine.
 * Manages adding and removing pages.
 * Maintains a state machine: IDLE, HEADER, ROW
 */
void domag(void)
{
	const uint16_t MAXLINE=200;
	char header[32];
	uint16_t i;
	uint8_t txListIndex;	// The current page
	uint8_t txListStart;	// Help prevent an endless loop
	PAGE* txList[256];	// One pointer per page. There are 256 possible pages in a magazine
	uint8_t mag;
	uint8_t row;
	uint8_t state;
	uint8_t packet[PACKETSIZE];
	PAGE* page=NULL;
	FILE* fil=NULL;
	time_t txwait=0;
	uint8_t isCarousel;
	PAGE carPage;
	
	CAROUSEL carousel[MAXCAROUSEL];		// Is 16 enough carousels? If not then change this yourself.
	char str[MAXLINE];
	// Init the transmission list for this magaine
	for (i=0;i<256;i++)
		txList[i]=NULL;
	// Work out which magazine we are
	mag=getMag();
	// Find the pages for this mag and put them into the transmission list.
	piLock(1);
	if (getList(txList,mag,carousel))
	{
		printf("Could not find pages on stream %1d       \n",mag);
		piUnlock(1);
		return;
	}
	/*
	 printf("[domag] Carousels found on mag %d\n",mag);
	
	for (i=0;i<MAXCAROUSEL;i++)
	{
		if (carousel[i].page)
			printf("[domag] mag=%d pageaddr=%d\n",mag,carousel[i]);
	}
	*/
	piUnlock(1);
	// printf("Mag thread is initialised: mag=%d\n",mag);
	
	// Initialise the magazine state
	state=STATE_BEGIN;
	// Start at page 0
	txListIndex=0;
	// The mag loop has a page counter that steps through all the pages
	// There is a state variable which helps step through the file.
	while(1)
	{
		// printf("Here is a mag thread, mag=%d\n",mag);
		while (bufferIsFull(&magBuffer[mag])) delay(20); // ms
		switch (state)
		{
		case STATE_BEGIN:	// First time only
			// do stuff the first time
			state=STATE_IDLE;
			break;
		case STATE_IDLE:	// Ready to start a new page
			// Find the next page to transmit
			isCarousel=0;
			// Timed carousel pages have priority	
			if (txwait<time(NULL))	// If we are due to transmit a carousel
			{
				txwait=pageToTransmit(carousel,&fil,&carPage,mag);
				if (txwait==0)
				{
					// printf("[domag] mag=%d NULL time returned. Adding 10 second wait\n",mag);
					// If we are expecting a carousel but only this happens, then PageToTransmit is broken
					txwait=time(NULL)+10;					
				}
				else
				{
					// If we get here then fp has a valid carousel file
					isCarousel=1;
				}
			}
			// If we didn't get a page object from pageToTransmit, we get it from the main list
			if (!isCarousel) 
			{
				// Find the next page in the main sequence
				txListStart=txListIndex;	// Avoid infinite loop if we have no pages in mag, (should go to another idle state if this happens)
				txListIndex++;
				while(!txList[txListIndex] )
				{
					txListIndex++;	// This will automatically wrap, hence no range checking.
					if (txListStart==txListIndex)	// oops. This magazine has nothing to show
					{
						state=STATE_BEGIN;	
						// printf("[domag] Magazine %d contains no pages\n",mag);
						delay(1000);	// Might as well do nothing most of the time
						break;
					}				
				}
				// printf("[domag] selected file=%s\n",txList[txListIndex]->filename);
				// Now we have the found the next page we get ready to transmit it.
				page=txList[txListIndex];		// Get the page object
			}
			else
			{
				page=&carPage;	// The page is a carousel page
				// printf("[domag]R %s\n",carPage.filename);
			}
			// printf("Q page=%s\n",page->filename);
			if (page)	// If we found a page to transmit
			{
				str[0]=0;
				if (!fil)	// Carousel will already be scanned down to the page that we want
					fil=fopen(page->filename,"r");	// Open the Page file if it is not a carousel
				//else
				//	printf("[mag]Carousel filename=%s\n",page->filename);
				// TODO: If the user changes the page file AND the header doesn't match our stored one,
				// then we should note this fact and update the packet header.
				while (strncmp(str,"OL,",3) && !feof(fil))				// scan down to the rows
					fgets(str,MAXLINE,fil);

				if (feof(fil))	// Not found any lines
				{
					fclose(fil);
					fil=NULL;
					state=STATE_IDLE;
				}
				else	// This is the first output line. Parse it and process it.
				{					
					// TX the header
					//sprintf(header,"P%01d%02x %s",page->mag,page->page,page->filename);
					// Create the header. Note that we force parallel transmission by ensuring that C11 is clear
					PacketHeader((char*)packet,page->mag,page->page,page->subcode,page->control & ~0x0040,header);
					// The header packet isn't quite finished. stream.c intercepts headers and adds dynamic elements, page, date, network ID etc.
					
					while (bufferPut(&magBuffer[mag],(char*)packet)==BUFFER_FULL) delay(20); 
						state=STATE_HEADER;
				}
			}
			else
				state=STATE_IDLE;	// We found nothing to send
			// Change to sending.
			break;
		case STATE_HEADER:	// If regional options are set, send the enhancement packet
/** @todo I suspect that the page enhancements are reversed or something.
 *  Also the RE command appears to be in hex which probably isn't what we want.
 */
			if (page->region>0 && page->region<=0x0f) // Only send this packet if the page asks for it with a non zero RE command
			{	
				// printf("Sending page region=%d\n",page->region);
				// We need to send the X/28 packet first (maybe???) so here would be a good place to do it.
				PageEnhancementDataPacket((char*) packet, page->mag, 28,0);
				int triplet=0;

				// Lets assemble triple 1 ETSI 300706 page 30. Also see section 9.4.2.1
				// 1..4 Page function. We set this to 0 as a standard teletext page. 
				// 5..7 Page coding. Set this to zero for 7 bit coding.
				// 8..14 G0/G2/Nat opt.  10,9,8 should be set to C12, C13,C14 TODO. Bits are in 14..11
				triplet+=page->region<<10; // Not reversed
				//if (page->region & 0x01) triplet|=0x40; // Reverse the bit order and shift up 7
				//if (page->region & 0x02) triplet|=0x20;
				//if (page->region & 0x04) triplet|=0x10;
				//if (page->region & 0x08) triplet|=0x08;
				// 15..18 Second G0

				SetTriplet((char*) packet, 1, triplet);
				// Should we also set triplets 1 to 13?
				for (i=2;i<=13;i++)
					SetTriplet((char*) packet, i, 0);
				Parity((char*)packet,50);	// 50 ensures that we only reverse bytes. Parity would mess up Ham24/8
				while(bufferPut(&magBuffer[mag],(char*)packet)==BUFFER_FULL) delay(20);
				// dumpPacket(packet);
			}
			// Now we can process the initial row of the page
			row=copyOL((char*)packet,str);
			if (row) // If this happens to be OL,0 then don't process packet
			{
				PacketPrefix((uint8_t*)packet, page->mag, row);
				Parity((char*)packet,5);				
				//dumpPacket(packet);
				while (bufferPut(&magBuffer[mag],(char*)packet)==BUFFER_FULL) delay(20);
			}		
			state=STATE_SENDING;	// Intentional fall through
		case STATE_SENDING:	// Transmitting rows
			fgets(str,MAXLINE,fil);
			// printf("[domag] Send a row %s\n",str);
			if (str[0]=='O' && str[1]=='L')	// Double check it is OL. It could be FL.
			{
				row=copyOL((char*)packet,str);
				if (row)	// Only insert a valid row
				{
					PacketPrefix(packet,page->mag,row);			
					Parity((char*)packet,5);
					while (bufferPut(&magBuffer[mag],(char*)packet)==BUFFER_FULL) delay(20);
				}
			}
			if (str[0]=='F' && str[1]=='L')	// Fastext links?
			{
				copyFL((char*)packet,str,page->mag);
				PacketPrefix(packet,page->mag,27); // X/27/0					
				Parity((char*)packet,5);
				while (bufferPut(&magBuffer[mag],(char*)packet)==BUFFER_FULL) delay(20);
			}
			// When we run out of rows to send
			if (fil)
			{
				if (feof(fil) || (str[0]=='S' && str[1]=='C'))
				{
					state=STATE_IDLE;
					fclose(fil);	// TODO: Don't try to close already closed!
					fil=NULL;
				}
				break;			
			}
		} // state switch
		delay(1);	// Just something to break up the sequence
		// TODO: We should really intercept a shutdown and release all the memory
	} // while
} // domag


/** MagInit creates eight domag threads
 * It also sets up the buffers that each thread will use to forward packets.
 */
void magInit(void)
{
	int i;
	const int maxThreads=8; 	// should be 8
	magCount=1;
	for (i=0;i<9;i++) // One extra buffer for Newfor
	{
		// Set up the buffers, one per thread
		bufferInit(&magBuffer[i],(char*)&magPacket[i],PACKETCOUNT);
		// now got to add the packet data itself
	}
	for (i=0;i<maxThreads;i++) {
		magThread[(i+1)%8]=0;
		pthread_create(&magThread[(i+1)%8],NULL,(void*)domag,(void*)&r1); 	// r1 is just a dummy arg.
		// printf("magInit %d done\n",i);
	}
} // magInit


