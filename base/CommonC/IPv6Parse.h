// Parse / Print IPv6 Addresses: IPv6Parse.h
/*
 * TSAR (Tools Slightly Above the Runtime)
 * Filename: IPv6Parse.h
 *
 * Copyright (c) 2026 International Business Machines Corporation
 * Copyright (c) 2008 Eric Kass
 *
 * SPDX-License-Identifier: MIT
 */
//

#ifndef __Print_Parse_IPv6

        #define __Print_Parse_IPv6

#include <SocketCB.h>

bool ParseIPv6(socketCBaddr_t &Addr, const char *Src);

int SprintIPv6(char Dest[40], const socketCBaddr_t &Addr);

int SprintIPv6Port(char Dest[46], const socketCBaddr_t &Addr);

// **************************************
// **** SOCKADDR to String Converter ****
// **************************************

class _SOCKADDR2String
        {
        private:
                char String[64];
                bool Init(const sockaddr *sockaddr, bool wPort);
        public:
                _SOCKADDR2String(const sockaddr *a, bool wPort=true)
                        {
                        Init(a,wPort);
                        }
                _SOCKADDR2String(const sockaddr_in *a, bool wPort=true)
                        {
                        Init((const sockaddr *)a,wPort);
                        }
                _SOCKADDR2String(const socketCBaddr_t *a, bool wPort=true)
                        {
                        Init((const sockaddr *)a,wPort);
                        }
                operator const char* () {return String;}
        };

#define SOCKADDR2String (const char *)_SOCKADDR2String

// **********************************************
// **** Map IPv4 to IPv6 mapped IPv4 Address ****
// **********************************************
//
// Note: Source and Destination can overlap.
//
// **********************************************

void MapIPv4toIPv6(socketCBaddr_t *AddrV6, socketCBaddr_t *AddrV4);

bool MapIPv6toIPv4(socketCBaddr_t *AddrV4, socketCBaddr_t *AddrV6);

#define isIPv6MappedIPv4(AddrV6) MapIPv6toIPv4(NULL,AddrV6)

/* ***************************************************************************
 Notes:

******************************************************************************
**** Example: ****************************************************************
***************
******************************************************************************

*****************************************************************************/

#endif
