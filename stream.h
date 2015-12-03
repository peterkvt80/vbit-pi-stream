#ifndef _STREAM_H_
#define _STREAM_H_

//#include <fcntl.h>
//#include <sys/ioctl.h>
//#include <linux/spi/spidev.h>
//#include <time.h>

#include <stdio.h>
//#include <stdlib.h>
//#include <unistd.h>
#include <stdint.h>

//#include <stdint.h>

#include "thread.h"

// VBIT
#include "buffer.h"
#include "mag.h"
#include "delay.h"


/** Stream is a thread that 
1) sinks packets from mag.c
2) Handles packet priority and sequencing
3) inserts special packets 8/30, databroadcast etc.
4) sources packets to outputstream.c
*/
PI_THREAD (Stream);
#define STREAMBUFFERSIZE 20
extern bufferpacket streamBuffer[1];

#endif
