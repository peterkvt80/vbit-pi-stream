/** Stream.c ************************
 * Thread that creates a stream of packets 
 * Most packets are taken from the mazagine threads
 * Other packets are generated as needed:
 * Packet 8/30, subtitles, databroadcast
 * Packets are sequenced by mag priority, primary and secondary actions.
 * They also interact with the mag state engines to ensure that headers
 * and their rows don't appear on the same field.
 * Sources packets from mag.c
 * Sinks packets to FillFIFO.c 
 * Copyright (c) 2013-2015 Peter Kwan. All rights reserved.
 */
#include "stream.h"

// #define _DEBUG_

// The stream buffer is 20 packets STREAMBUFFERSIZE

// This should be 9. We want to add the subtitle streams
#define STREAMS 9

bufferpacket streamBuffer[1];//  
uint8_t streamPacket[STREAMBUFFERSIZE*PACKETSIZE*2]; // The 2 is extra storage to find out what is making it crash
// The lower the priority number, the faster the magazine runs.
// This way you can choose which mags are more important.
//                   mag 8 1 2 3 4 5 6 7
static char priority[STREAMS]={5,3,3,3,3,2,5,6,1};	// 1=High priority,9=low. Note: priority[0] is mag 8, while priority mag[8] is the newfor stream!
static char priorityCount[STREAMS];

PI_THREAD (Stream)
{
	int mag=0;
	uint8_t line=16; // start at 16, will get rolled over to 0 immediately
	uint8_t i;
	uint8_t result;
	uint8_t holdCount=0;	/// If the entire service is on hold, we need to add lines, either filler or quiet
	uint8_t field=0;	/// Count fields
	uint8_t hold[STREAMS];	/// If hold is set then we must wait for the next field to reset them
	uint32_t skip=0;	// How many lines we were unable to put real packets on
	uint8_t packet[PACKETSIZE];	
	for (i=0;i<STREAMS;i++)
	{
		hold[i]=0;
		priorityCount[i]=priority[i];
	}
	// Initialise the stream buffer
	bufferInit(streamBuffer,(char*)streamPacket,STREAMBUFFERSIZE);
	delay(500);	// Give the other threads a chance to get started
	// Poll the input buffers and hold back until there is data ready to go 
	for (mag=0;!bufferIsEmpty(&magBuffer[mag]);mag=(mag+1)%8) delay(10);
	while(1)
	{
		// printf("Hello from stream\n");
		// delayMicroseconds(100);
		
		// make it go next. It could pretend to be magazine 9. 
		// You must not put any pages on that magazine or they may be corrupted.

		// If there is ANYTHING in the subtitle buffer it goes immediately, as long as we are not waiting for the next field
		if (!bufferIsEmpty(&magBuffer[8]) && !hold[8] && FALSE) // subtitle buffer is smashing the stack
		{
			mag=8;
			priorityCount[0]=32; // Also delay mag 8 for two fields so it doesn't clash
		}
		else
		// Decide which mag can go next
		for (;priorityCount[mag]>0;mag=(mag+1)%STREAMS)
		{
			priorityCount[mag]--;
		}
		// This scheme more or less works but there is no guarantee that it stays in sync.
		// So we need to ensure that FillFIFO knows what this phase is and matches it to the output.
		// The 7120/7121 DENC only does up to 16 lines on both fields. Line 17 is not available for us :-(
		// The lines and fields should stay synchronised so long as nothing increments a line without 
		// actually pushing a packet onto the buffer.
		// (carousel pages breaking in over normal pages)
		if (line>=16)
		{
			line=0;
			for (i=0;i<STREAMS;i++) hold[i]=0;	// Any holds are released now
			
			// may as well insert packet 8/30 on first line of vbi
			if(field>=50) field = 0; // loop field counter every second
			
			switch (field)
			{
				case 0:
					// packet 8/30 format 1
					// this should occur during the first vbi following a clock second, but we're buffering stuff anyway so there's no point even trying to synchronise that finely
					Packet30(packet, 1, serviceStatusString);
					while(bufferPut(streamBuffer,(char*)packet)!=BUFFER_OK) delay(10); // force this to be added to buffer
					//fprintf(stderr, "[stream] inserting 8/30 f1 in field %d line %d\n",field,line);
					line++;
					break;
				
				case 10:
					// packet 8/30 format 2 LC 0
					//fprintf(stderr, "[stream] not inserting 8/30 f2 in field %d line %d\n",field,line);
					break;
				
				case 20:
					// packet 8/30 format 2 LC 1
					//fprintf(stderr, "[stream] not inserting 8/30 f2 in field %d line %d\n",field,line);
					break;
				
				case 30:
					// packet 8/30 format 2 LC 2
					//fprintf(stderr, "[stream] not inserting 8/30 f2 in field %d line %d\n",field,line);
					break;
				
				case 40:
					// packet 8/30 format 2 LC 3
					//fprintf(stderr, "[stream] not inserting 8/30 f2 in field %d line %d\n",field,line);
					break;
			}
			
			field++;
		}

		if (!hold[mag])
		{
			holdCount=0;
			// Pop a packet from a mag and push it to the stream		
			// dumpPacket(magBuffer[mag].pkt);
			result=bufferMove(streamBuffer,&(magBuffer[mag]));

			switch (result)
			{
			case BUFFER_FULL: 	// Buffer full. This is good because it means that we are not holding things up
			
				//delay(1);	// Hold so we can see the message (WiringPi)
				break;
			case BUFFER_EMPTY: 	// Source not ready. We expect mag to send us something very soon
				// If a stream has no pages, this branch will get called a lot
				
				hold[mag]=1;	// Might as well put it in hold. If this isn't here then the mag can grab ALL the packets
				// delay(1);	// Hold so we can see the error
				break;
			case BUFFER_HEADER:		// Header row
				// fprintf(stderr,"%01d",mag);
				hold[mag]=1;
				// Intentional fall through
			case BUFFER_OK:  // Normal row
				line++;	// Count up the lines sent
				break;				
			}
			// TODO: If ALL fields are on hold we need to output filler or quiet
			// TODO: Implement stream side buffer
		}
		else
		{	// If there really is nothing to send, send a filler or quiet
			holdCount++;
			if (holdCount>=8) 	// all mags are in nip?
			{
				skip++;
				//printf("[stream] inserting quiet %d\n",skip);	// You can remove this line. It is for debugging
				// TODO: If this message comes up,
				// Then add quiet or filler 8/25 packets.
				// This RARELY happens except at the start
				PacketQuiet(packet);
				if(bufferPut(streamBuffer,(char*)packet) == BUFFER_OK) line++;;
			}
		}
		// printf("[Stream] mag=%d\n",mag); 
		if (priority[mag]==0) priority[mag]=1;	// Can't be 0 or that mag will take all the packets
		priorityCount[mag]=priority[mag];	// Reset the priority for the mag that just went out
		//else
		//	printf("[Stream] mag buffer %d is empty\n",mag);
	}
} 
