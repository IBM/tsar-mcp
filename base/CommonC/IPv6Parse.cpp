// Parse / Print IPv6 Addresses: IPv6Parse.cpp
/*
 * TSAR (Tools Slightly Above the Runtime)
 * Filename: IPv6Parse.cpp
 *
 * Copyright (c) 2026 International Business Machines Corporation
 * Copyright (c) 2008 Eric Kass
 *
 * SPDX-License-Identifier: MIT
 */
//

#include <stdio.h>
#include <stdlib.h>

#include <SocketCB.h>
#include <LevelTrace.h>
#include <IPv6Parse.h>

#ifndef _Network_Trasmission
        #ifdef _WIN32
                #define _WSPIAPI_H_     // Can't compile <wspiapi.h> on VC6.
                #include <winsock2.h>
                #include <Ws2tcpip.h>
        #else /* __UNIX__ */
	        #include <sys/socket.h>
	        #include <sys/types.h>
	        #include <netinet/in.h>
                #include <netinet/tcp.h>
        #endif
        typedef sockaddr_in6 socketCBaddr_t;
#endif

#ifdef  __OS400_TGTVRM__
        #pragma convert(500)
#endif

typedef unsigned short IPv6Words_t[8];

#define decval(x) ((x) - '0')

#define isdecdigit(x) (((x) >= '0' && (x) <= '9'))

#define hexval(x) (((x) >= '0' && (x) <= '9')                               \
                   ? (x) - '0'                                              \
                   : ((x) >= 'A' && (x) <= 'F')                             \
                     ? (x) - 'A' + 10                                       \
                     : (x) - 'a' + 10)

#define ishexdigit(x) (((x) >= '0' && (x) <= '9') ||                        \
                       ((x) >= 'A' && (x) <= 'F') ||                        \
                       ((x) >= 'a' && (x) <= 'f'))


#define IPv4PORTSeperator ':'
#define IPv6PORTSeperator '|'
#define HexGROUPSeperator ':'
#define DecGROUPSeperator '.'

static bool ParseIPv6(IPv6Words_t Words, unsigned *Port, const char *Src)
        {
        unsigned jByte = 0;
        unsigned iWord = 0;
        unsigned iZeroSpan = 0;
        unsigned short dWord = 0;
        unsigned short hWord = 0;
        bool WaitingForPort = false;
        char PORTSeperator = IPv6PORTSeperator;
        if (Port) *Port = 0;
        while (*Src)
                {
                char c = *Src++;
                if (c == '[') {}                // Format: [Addr]:Port
                else if (c == ']')
                        {
                        PORTSeperator = IPv4PORTSeperator;
                        }
                else if (c == PORTSeperator || 
                         c == HexGROUPSeperator || 
                         c == DecGROUPSeperator)
                        {
                        if (WaitingForPort) break;
                        else if (jByte) 
                                {
                                if (iWord == 8) return false;
                                Words[iWord] = (Words[iWord] << 8) + dWord;
                                iWord++;
                                jByte = 0;
                                }
                        else if (c == DecGROUPSeperator)
                                {
                                Words[iWord] = dWord;
                                jByte = 1;
                                }
                        else    {
                                if (iWord == 8) return false;
                                Words[iWord++] = hWord;
                                }
                        hWord = 0;
                        dWord = 0;
                        if (c == PORTSeperator)
                                {
                                WaitingForPort = true;
                                }
                        else if (*Src == HexGROUPSeperator) 
                                {
                                iZeroSpan = iWord;
                                Src++;
                                }
                        }
                else if (ishexdigit(c)) 
                        {
                        hWord = (hWord << 4) + hexval(c);
                        if (isdecdigit(c))
                                {
                                dWord = (dWord * 10) + decval(c);
                                }
                        else if (jByte) return false;
                        }
                else return false;
                }
        if (WaitingForPort && Port) *Port = dWord; 
        else if (iWord == 8) return false;
        else if (jByte) 
                {
                Words[iWord] = (Words[iWord] << 8) + dWord; 
                iWord++; 
                }
        else Words[iWord++] = hWord;
        for (unsigned i=0; i < 8 - iZeroSpan; i++)
                {
                if (i < iWord-iZeroSpan) Words[7-i] = Words[iWord-1-i];
                else Words[7-i] = 0;
                }
        return true;
        }

bool ParseIPv6(socketCBaddr_t &Addr, const char *Src)
        {
        unsigned Port = 0;
        IPv6Words_t Words;
        bool Status = ParseIPv6(Words,&Port,Src);
        if (Status)
                {
                for (unsigned i=0; i < 8; i++) 
                        {
  #ifdef _Network_Trasmission
                        Addr.in6.in6_addr.u.Word[i] = htons(Words[i]);
  #else
                        Addr.sin6_addr.u.Word[i] = htons(Words[i]);
  #endif
                        }
  #ifdef _Network_Trasmission
                Addr.in.port = htons((unsigned short)Port);
                Addr.in.family = AF_INET6;
  #else
                Addr.sin6_port = htons((unsigned short)Port);
                Addr.sin6_family = AF_INET6;
  #endif
                }
        return Status;
        }

static int SprintIPv6(char Dest[40], const IPv6Words_t Words)
        {
        char *Pos = Dest;
        bool ZeroSpan = false;
        for (unsigned i=0; i < 7; i++)
                {
                if (!ZeroSpan && Words[i] == 0)
                        {
                        unsigned j = i + 1;
                        while (j < 7 && Words[j] == 0) j++;
                        if (j > i + 1)
                                {
                                *Pos++ = HexGROUPSeperator;
                                ZeroSpan = true;
                                i = j;
                                }
                        if (i == 7) break;
                        }
                if (i) *Pos++ = HexGROUPSeperator;
                unsigned short Word = Words[i];
                Pos += sprintf(Pos,"%X",Word);
                }
        *Pos++ = HexGROUPSeperator;
        unsigned short Word = Words[7];
        Pos += sprintf(Pos,"%X",Word);
        return (int)(Pos - Dest);
        }

int SprintIPv6(char Dest[40], const socketCBaddr_t &Addr)
        {
        IPv6Words_t Words;
  #ifdef _Network_Trasmission
        const unsigned short *AddrWords = Addr.in6.in6_addr.u.Word;
  #else
        const unsigned short *AddrWords = Addr.sin6_addr.u.Word;
  #endif
        for (unsigned i=0; i < 8; i++) 
                {
                Words[i] = ntohs(AddrWords[i]);
                }
        return SprintIPv6(Dest,Words);
        }

int SprintIPv6Port(char Dest[46], const socketCBaddr_t &Addr)
        {
  #ifdef _Network_Trasmission
        unsigned short Port = ntohs(Addr.in.port);
  #else
        unsigned short Port = ntohs(Addr.sin6_port);
  #endif
        int rc = SprintIPv6(Dest,Addr);
        if (rc > 0 && rc < 40)
                {
                rc += sprintf(&Dest[rc],"%c%u",IPv6PORTSeperator,Port);
                }
        return rc;
        }

// ***************************************************************************
// **** SOCKADDR to String Converter *****************************************
// ***************************************************************************

static void SprintIPv4(char *AddrString, const sockaddr_in *Address)
        {
        unsigned char *b = (unsigned char *)&Address->sin_addr;
        sprintf(AddrString,"%u.%u.%u.%u",b[0],b[1],b[2],b[3]);
        return;
        }

static void SprintIPv4Port(char *AddrString, const sockaddr_in *Address)
        {
        unsigned char *b = (unsigned char *)&Address->sin_addr;
        unsigned Port = ntohs(Address->sin_port);
        sprintf(AddrString,"%u.%u.%u.%u:%u",b[0],b[1],b[2],b[3],Port);
        return;
        }

bool _SOCKADDR2String::Init(const sockaddr *sockaddr, bool wPort)
        {
        static const char *ProcName = "SOCKADDR2String:Init";
        if (!sockaddr)
                {
                String[0] = '\0';
                return true;
                }
        if (sockaddr->sa_family == AF_INET)
                {
                if (!wPort) SprintIPv4(String,(const sockaddr_in *)sockaddr);
                else SprintIPv4Port(String,(const sockaddr_in *)sockaddr);
                }
        else if (sockaddr->sa_family == AF_INET6)
                {
                socketCBaddr_t *AddrV6 = (socketCBaddr_t *)sockaddr;
                u_short *w6 = (u_short *)&AddrV6->in6.in6_addr;
                u_char *b6 = (u_char *)&AddrV6->in6.in6_addr;
                if (w6[5] == 0xFFFF &&
                    w6[0] == 0 &&
                    w6[1] == 0 &&
                    w6[2] == 0 &&
                    w6[3] == 0 &&
                    w6[4] == 0)                 // IPv6 mapped IPv4:
                        {
                        socketCBaddr_t AddrV4;
                        AddrV4.in.family = AF_INET;
                        u_char *b4 = (u_char *)&AddrV4.in4.sin_addr;
                        b4[0] = b6[12];
                        b4[1] = b6[13];
                        b4[2] = b6[14];
                        b4[3] = b6[15];
                        AddrV4.in.port = AddrV6->in6.port;
                        if (!wPort) SprintIPv4(String,(sockaddr_in *)&AddrV4);
                        else SprintIPv4Port(String,(sockaddr_in *)&AddrV4);
                        }
                else if (!wPort)
                        {
                        SprintIPv6(String,*(const socketCBaddr_t *)sockaddr);
                        }
                else SprintIPv6Port(String,*(const socketCBaddr_t *)sockaddr);
                }
        else    {
                String[0] = '0';
                String[1] = '\0';
                }
        return true;
        }

// ***************************************************************************
// **** Map IPv4 to IPv6 mapped IPv4 Address *********************************
// *******************************************
//
// Note: Source and Destination can overlap.
//
// ***************************************************************************

void MapIPv4toIPv6(socketCBaddr_t *AddrV6, socketCBaddr_t *AddrV4)
        {
        static const char *ProcName = "MapIPv4toIPv6";
        if (!AddrV6 || !AddrV4) return;
        if (AddrV4->in.family != AF_INET)
                {
                TERROR(("%s: Invalid IPv4 Address: %s",
                        ProcName,
                        SOCKADDR2String(AddrV4)));
                return;
                }
        // ----------------                     // Function works if Dest
        // ---- Source ----                     // and Source are the same.
        // ----------------
        u_char *b4 = (u_char *)&AddrV4->in4.sin_addr;
        u_char b0 = b4[0];
        u_char b1 = b4[1];
        u_char b2 = b4[2];
        u_char b3 = b4[3];
        u_short port = AddrV4->in.port;
        // --------------
        // ---- Dest ----
        // --------------
        u_short *w6 = (u_short *)&AddrV6->in6.in6_addr;
        u_char *b6 = (u_char *)&AddrV6->in6.in6_addr;
        w6[0] = 0;
        w6[1] = 0;
        w6[2] = 0;
        w6[3] = 0;
        w6[4] = 0;
        w6[5] = 0xFFFF;
        b6[12] = b0;
        b6[13] = b1;
        b6[14] = b2;
        b6[15] = b3;
        AddrV6->in6.port = port;
        AddrV6->in.family = AF_INET6;
        return;
        }

bool MapIPv6toIPv4(socketCBaddr_t *AddrV4, socketCBaddr_t *AddrV6)
        {
        if (!AddrV6) return false;
        if (AddrV6->in.family != AF_INET6) return false;
        u_short *w6 = (u_short *)&AddrV6->in6.in6_addr;
        u_char *b6 = (u_char *)&AddrV6->in6.in6_addr;
        bool isMapped =  w6[5] == 0xFFFF &&
                         w6[0] == 0 &&
                         w6[1] == 0 &&
                         w6[2] == 0 &&
                         w6[3] == 0 &&
                         w6[4] == 0;                    // IPv6 mapped IPv4.
        if (!isMapped) return false;
        if (AddrV4)
                {
                u_char b12 = b6[12];
                u_char b13 = b6[13];
                u_char b14 = b6[14];
                u_char b15 = b6[15];
                u_short port = AddrV6->in6.port;
                // --------------
                // ---- Dest ----
                // --------------
                u_char *b4 = (u_char *)&AddrV4->in4.sin_addr;
                b4[0] = b12;
                b4[1] = b13;
                b4[2] = b14;
                b4[3] = b15;
                AddrV4->in.port = port;
                AddrV4->in.family = AF_INET;
                }
        return true;
        }

// ***************************************************************************
// ***************************** End of File *********************************
// ***************************************************************************
