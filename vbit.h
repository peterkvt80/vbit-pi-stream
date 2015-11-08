 /**	Parts:
  * Copyright (c) 2012 Gordon Henderson
  */
#ifndef _VBIT_H_
#define _VBIT_H_

// System
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/spi/spidev.h>
#include <time.h>

#include <sys/socket.h> /* for socket(), bind(), and connect() */
#include <arpa/inet.h>  /* for sockaddr_in and inet_ntoa() */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <stdint.h>

// Application

#include "packet.h"
#include "stream.h"
#include "mag.h"
#include "outputstream.h"

#include "thread.h"

extern void         delay             (unsigned int howLong) ;

#endif
