/** @file nu4.c
 * @brief Softel Newfor subtitles for VBIT.
 * A TCP port 5570 on VBIT accepts nu4 subtitling commands.
 * The main purpose is to play out pre-recorded subtititles.
 * You can also play out subtitles:
 * from a video server
 * bridged from another subtitles source such as through a DTP600.
 * or accept any industry standard Newfor source.
 */

#include "nu4.h" 

bufferpacket packetCache[1]; // Incoming rows are cached here, and transferred to subtitleBuffer when OnAir 

// Packet Subtitles are stored here 

uint8_t subtitleCache [SUBTITLEPACKETCOUNT][PACKETSIZE];	/// Storage for 8 packets, 45 bytes per packet. (for packetCache)

static uint8_t _page;	/// Page number (in hex). This is set by Page Init
static uint8_t _rowcount; /// Number of rows in this subtitle
static char packet[PACKETSIZE];

/** initNu4
 * @detail Initialise the buffers used by Newfor
 */
void InitNu4()
{  
	bufferInit(packetCache   ,(char*)subtitleCache ,SUBTITLEPACKETCOUNT);  
}


/** 
 * @detail Expect four characters
 * 1: Ham 0 (always 0x15)
 * 2: Page number hundreds hammed. 1..8
 * 3: Page number tens hammed
 * Although it says HTU, in keeping with the standard we will work in hex.
 */
int SoftelPageInit(char* cmd)
{
  int page=0;
  int n;
  
  
  // Useless leading 0
  n=vbi_unham8(cmd[1]);
  if (n<0) return 0x900; // @todo This is an error.
  // Hundreds
  n=vbi_unham8(cmd[2]);
  if (n<1 || n>8) return 0x901;
  page=n*0x100;
  // tens (hex allowed!)
  n=vbi_unham8(cmd[3]);
  if (n<0 || n>0x0f) return 0x902;
  page+=n*0x10;
  // units
  n=vbi_unham8(cmd[4]);
  if (n<0 || n>0x0f) return 0x903;
  page+=n;
  _page=page;
  return page;
}

void SubtitleOnair(char* response)
{
	// What page is the subtitle on? THIS IS IN THE WRONG PLACE! Ideally we will already have a header and placed it in the cache
	uint8_t mag;
	uint8_t page;
	mag=(_page/0xff) & 0x07;
	page=_page & 0xff;
	// First packet needs to be the header. Could well want suppress header too.
	sprintf(response,"[SubtitleOnair]mag=%1x page=%02x",mag,page);
	// PacketHeader(packet, mag, page, 0, 0x0002, "test");
	PacketHeader(packet, mag, page, 0, 0x4002); // Dummy
	// dumpPacket(packet);
	bufferPut(&magBuffer[8],packet);

	while (!bufferIsEmpty(packetCache))
	{
		bufferMove(&magBuffer[8],packetCache);
	}

}
void SubtitleOffair()
{
// Construct a header for _page with the erase flag set.
//  
	uint8_t mag;
	uint8_t page;
	mag=(_page/0xff) & 0x07;
	page=_page & 0xff;
	// Control bits are erase+subtitle
	PacketHeader(packet, mag, page, 0, 0x4002);
	Parity(packet,13);
	// May want to clear subtitle buffer at the same time
	bufferPut(&magBuffer[8],packet);
}

/**
 * @return Row count 1..7, or 0 if invalid 
 */
int GetRowCount(char* cmd)
{
  int n=vbi_unham8(cmd[1]);
  if (n>7)
	n=0;
  _rowcount=n;
  return n;	
}

/**
 * @brief Save a row of data when a Newfor command is completed
 */
void saveSubtitleRow(uint8_t mag, uint8_t row, char* cmd)
{
	char packet[PACKETSIZE]; // A packet for us to fill
	PacketPrefix((uint8_t*)packet, mag, row);
	// copy from cmd to the packet
	strncpy(packet+5,cmd,40);
	// fix the parity
	Parity(packet,5);
	// stuff it in the buffer
	bufferPut(packetCache,packet); // buffer packets locally

}
