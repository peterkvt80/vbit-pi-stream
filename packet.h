/** packet.h
 * VBIT on Raspberry Pi
 * header file for packet.c
 * 
 * Peter Kwan, Copyright 2013
 */

#ifndef _PACKET_H_
#define _PACKET_H_
#define PACKETSIZE 45
#include <stdio.h>
#include <stdint.h>
#include "tables.h"
#include <string.h>
#include <stdlib.h>     /* strtol */
#include "hamm.h"
#include "settings.h"

/** copyOL - Copy Output Line
 */
uint8_t copyOL(char *packet, char *textline);

/** copyFL - Copy the Fastext links
 */
void copyFL(char *packet, char *textline, unsigned char mag);

/** dumpPacket - Display the packet in lovely hexadecimal
 */
void dumpPacket(char* packet);

/** Also need to define a field of ...
 */

/** Check that parity is correct for the packet payload
 * The parity is set to odd for all bytes from offset to the end
 * Offset should be at least 3, as the first three bytes have even parity
 * The bits are then all reversed into transmission order
 * \param packet : packet to check
 * \param offset : start offset to check. (5 for rows, 13 for header)
 */ 
void Parity(char *packet, uint8_t offset);

/** PacketClear
 * \param packet : Byte array at least PACKETLENGTH bytes;
 * \param value : set all the bytes to this value
 */
void PacketClear(uint8_t *packet, uint8_t value);

/** PacketQuiet - Set the entire packet to 0. Remember not to do parity after this.
 * \param packet : Byte array at least PACKETLENGTH bytes;
 */
void PacketQuiet(uint8_t *packet);

/** Make a filler packet
 * \param packet : Byte array at least PACKETLENGTH bytes;
 */
void PacketFiller(uint8_t *packet);

/** Make a broadcast service data packet
 * \param packet : Byte array at least PACKETLENGTH bytes;
 * \param format : broadcase data service packet format (1 or 2)
 */
void Packet30(uint8_t *packet, uint8_t format);

/** PacketPrefixValue
 * Call this when starting a new packet.
 * \param packet : Byte array at least PACKETSIZE bytes;
 * \param mag : Magazine number 1..8
 * \param row : Row number 0..31
 * \param value : Value to fill the packet with 
 */
void PacketPrefixValue(uint8_t *packet, uint8_t mag, uint8_t row, int value);

/** PacketPrefix
 * Call this when starting a new packet.
 * Same as PacketPrefixValue with value set to 0.
 * \param packet : Byte array at least PACKETLENGTH bytes;
 * \mag : Magazine number 1..8
 * \row : Row number 0..31
 */
void PacketPrefix(uint8_t *packet, uint8_t mag, uint8_t row);
void PacketHeader(char *packet ,unsigned char mag, unsigned char page, unsigned int subcode, unsigned int control);

extern volatile uint32_t UTC; // 10:00am

/** PageEnhancementDataPacket used in VBIT initially for language codepage selection See ETSI 300706
 *  Create a data packet for X/26, X/28, M/29
 * \param packet : A pointer to a teletext data packet 
 * \param mag : Magazine number, the mag number. This is the mag number in the page you are sending
 * \param row : Row can be 26, 28 or 29
 * \param designationCode : The packet format number 0..3. 
 */
void PageEnhancementDataPacket(char *packet, int mag, int row, int designationCode);

/** SetTriplet sets a triplet in the packet.
 *  Can also be used for X/1 to X/25 POP and GOP
 *  Does ham 24/18 of the three bytes. (out of 24 bits supplied, 18 are data, the rest are HAM protection)
 *  Do NOT apply parity or byte reversing to this. This would wreck the Ham24/18 coding
 *  \param ix : Index of the triplet 1..13
 *  \param triplet: An 18 bit value. 0..0x3ffff 
 */
void SetTriplet(char *packet, int ix, int triplet);


#endif

