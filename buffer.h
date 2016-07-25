#ifndef _BUFFER_H_
#define _BUFFER_H_

/** buffer
 * Routines that accept and store text packets, then recall them.
 * Used to buffer output of mag.c
 * and stream.c
 * Some sort of copyright here.
 */
 
#include <stdio.h>
#include <time.h>

#include "packet.h"
// Buffer structures are first packet, last packet, head and tail index
// Access methods init, push, pop, isempty,isfull

// Buffer return constants
#define BUFFER_OK 0
#define BUFFER_FULL 1
#define BUFFER_EMPTY 2
#define BUFFER_HEADER 3
// Not sure we need these. The source and destination are implied
//#define BUFFER_DESTINATION_FULL 4
//#define BUFFER_SOURCE_EMPTY 4

/** buffer control block
 * Contains all the data needed for a circular buffer of packets
 * All units are packet counts, not addresses
 * There is one buffer for the stream.c packet multiplexer with at least a fields worth of packets.
 * and one buffer for each magazine with enough storage for a ttx page.
 */
typedef struct  {
	char* pkt;			// The address of the packet buffer. (This must be allocated separately)
	uint8_t count;		// The total number of packets in that buffer
	uint8_t head;		// The head of the buffer (next to push)
	uint8_t tail;		// Tail of the buffer (next to pop)
} bufferpacket;

extern char headerTemplate[]; // template string for generating header packets

/* meta packet values */
//#define META_PACKET_HEADER 0
//#define META_PACKET_ODD_START 1
//#define META_PACKET_EVEN_START 2

// Functions

/**bufferInit
 * Sets up a packet buffer
 * \param bp - A bufferpacket control block
 * \param buf - The address of the packet buffer
 * \param len - The number of packets in the buffer
 */
void bufferInit(bufferpacket *bp, char *buf, uint8_t len);

/**bufferPut
 * Push packet pkt onto bufferpacket bp.
 * \param pkt : Packet to push
 * \param bp : buffer to push the packet onto
 * \return 0 if OK 1 if full.
 */
uint8_t bufferPut(bufferpacket *bp, char *pkt);

/**bufferGet
 * Get packet pkt from the tail of bufferpacket bp.
 * \param pkt : Packet to accept pop
 * \param bp : buffer to pop packet from
 * \return 0 if OK 1 if empty.
 */
uint8_t bufferGet(bufferpacket *bp, char *pkt);

/**bufferIsEmpty
 * Test whether a buffer is empty
 * \param bp : buffer to test
 * \return 0 if not empty 1 if empty.
 */
uint8_t bufferIsEmpty(bufferpacket *bp);

/**bufferIsFull
 * Test whether a buffer is full
 * \param bp : buffer to test
 * \return 0 if not empty 1 if full
 */
uint8_t bufferIsFull(bufferpacket *bp);

/** buffermove
 * Pops from buffer b2 and pushes to b1
 * This might be handy where it comes to multiplexing mag to stream.
 */
  /** bufferMove
  * Pops from buffer b2 and pushes to b1
  * This might be handy where it comes to multiplexing mag to stream.
  * \param b1 Destination buffer
  * \param b2 Source buffer
  * \return 0=OK, 1=failed 2=header
  */
uint8_t bufferMove(bufferpacket *b1, bufferpacket *b2);

/** bufferLevel - Used to work out approximately what line we are on
 * \return The number of packets in the buffer
 */
uint8_t bufferLevel(bufferpacket *bp);



#endif
