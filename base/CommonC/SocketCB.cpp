// Socket Control Block: SocketCB.cpp
/*
 * TSAR (Tools Slightly Above the Runtime)
 * Filename: SocketCB.cpp
 *
 * Copyright (c) 2026 International Business Machines Corporation
 * Copyright (c) 1995 Eric Kass
 *
 * SPDX-License-Identifier: MIT
 */
//

#if defined(_AIX) || defined(__OS400_TGTVRM__)
        #pragma priority(-4200)
#endif

#include <assert.h>
#include <errno.h>
#include <string.h> 

#if !defined(__WIN32__) && defined(_WIN32)
        #define __WIN32__
#endif

#ifdef __WIN32__

        #if defined(_NOAventail_Socket)
                #define USE_WINSOCK2 1
        #elif defined(SOCKETCB_IPV6)
                #define USE_WINSOCK2 1
        #elif !defined(USE_WINSOCK2)
                #define USE_WINSOCK2 0
        #endif

        #if USE_WINSOCK2
                #if _MSC_VER == 1200    // Can't compile <wspiapi.h> on VC6.
                        #define _WSPIAPI_H_
                #endif
                #include <winsock2.h>
                #include <Ws2tcpip.h>
        #else
                #include <winsock.h>
        #endif
#endif

#ifndef __WIN32__ /* __UNIX__ */

        #include <sys/time.h>
        #include <arpa/inet.h>
        #include <netdb.h>
        #include <fcntl.h>
        #include <stdlib.h>
        #include <unistd.h> 

#endif /* __UNIX__ */

#include <LevelTrace.h>
#include <IPv6Parse.h>
#include <SocketCB.h>

#ifndef __WIN32__ /* __UNIX__ */

        #if !defined(__OS400_TGTVRM__) && !defined(stricmp)
                /* REDEFSTDFUNC */
                #define stricmp strcasecmp
        #endif
        typedef sockaddr* LPSOCKADDR;
        typedef int REUSEADDR_t;
        typedef int TCPNODELAY_t;
        typedef int SOKEEPALIVE_t;
        typedef int IPV6ONLY_t;

        #if defined(__OS400__) && (__OS400_TGTVRM__ <= 510)
                typedef int socklen_t;
        #endif /* __OS400__ */

        #if !defined(FNDELAY) && defined(O_FNDELAY)
                #define FNDELAY O_FNDELAY
        #endif

        #if !defined(IPV6_V6ONLY)
                #if defined(__OS400__)
                        #define IPV6_V6ONLY 100
                #elif defined(_AIX)
                        #define IPV6_V6ONLY 37
                #else // Linux
                        #define IPV6_V6ONLY 26
                #endif
        #endif

#else  /* __WIN32__ */

        typedef int socklen_t;
        typedef BOOL REUSEADDR_t;
        typedef BOOL TCPNODELAY_t;
        typedef BOOL SOKEEPALIVE_t;
        typedef int IPV6ONLY_t;

        #ifndef IPPROTO_IPV6
                #define IPPROTO_IPV6 41
        #endif
        #ifndef IPV6_V6ONLY
                #define IPV6_V6ONLY 27
        #endif

#endif /* __WIN32__ */

#include <ASThread.h>

#ifndef SD_BOTH
        #define SD_BOTH 2
#endif

// ****************************************************************************
// **** Socket Initialization *************************************************
// ****************************************************************************

static Mutex SocketInitMutex;
static unsigned SocketInitCount = 0;

bool InitializeSockets()
        {
        static const char *ProcName = "InitializeSockets";
        bool Status = true;
        SocketInitMutex.Take();
  #ifdef __WIN32__
        if (!SocketInitCount)
                {
                int err;
                WSADATA wsaData;
                WORD VersionAccepted = 0x0101;
                WORD VersionRequested = 0x0002;
                err = WSAStartup(VersionRequested, &wsaData);
                if (err != 0) Status = false;
                else if (LOBYTE(wsaData.wVersion) < LOBYTE(VersionAccepted)
                         ||
                         (LOBYTE(wsaData.wVersion) == LOBYTE(VersionAccepted) 
                          &&
                          HIBYTE(wsaData.wVersion) < HIBYTE(VersionAccepted)))
                        {
                        WSACleanup();
                        Status = false;
                        }
                if (!Status)
                        {
                        TERROR(("%s: Winsock WSAStartup() Failure",ProcName));
                        MessageBeep(MB_ICONEXCLAMATION);
                        SocketInitMutex.Release();
                        exit(EXIT_FAILURE);
                        }
                }
  #endif
        SocketInitCount++;
        SocketInitMutex.Release();
        return Status;
        }

void DeinitializeSockets()
        {
        static const char *ProcName = "DeinitializeSockets";
        SocketInitMutex.Take();
        assert(SocketInitCount);
        SocketInitCount--;
  #ifdef __WIN32__
        if (!SocketInitCount) 
                {
                int rc = WSACleanup();
                if (rc) TERROR(("%s: WSACleanup() Failed (%d)",
                                ProcName,
                                WSAGetLastError()));
                unsigned ForceCount = 0;                // Other DLLs may 
                while (WSACleanup() == 0) ForceCount++; // have also called
                if (ForceCount)                         // WSAStartup().
                        {
                        TDEBUG(("%s: WARNING: WSACleanup() Forced",ProcName));
                        }
                }
  #endif
        SocketInitMutex.Release();
        return;
        }

// ****************************************************************************
// **** Address utilities *****************************************************
// ****************************************************************************

#ifdef SOCKETCB_IPV6

// **************************
// ********** IPv6 **********
// **************************

static flag ResolveAddress(socketCBaddr *AddrDest, int AddressFamily,
                                                   const char *Address, 
                                                   const char *Service,
                                                   const char *Protocol,
                                                   int *LastError)
        {
        addrinfo HintsV4 = {0};
        addrinfo HintsV6 = {0};
        addrinfo *Results = NULL;
        if (!AddrDest) return false;
        memset(AddrDest,0,sizeof(socketCBaddr));
        HintsV4.ai_family = AF_INET;
        HintsV6.ai_family = AF_INET6;
        if (Protocol && stricmp(Protocol,"udp") == 0) 
                {
                HintsV4.ai_socktype = HintsV6.ai_socktype = SOCK_DGRAM;
                HintsV4.ai_protocol = HintsV6.ai_protocol = IPPROTO_UDP;
                }
        else if (Protocol && stricmp(Protocol,"tcp") == 0)
                {
                HintsV4.ai_socktype = HintsV6.ai_socktype = SOCK_STREAM;
                HintsV4.ai_protocol = HintsV6.ai_protocol = IPPROTO_TCP;
                }
        else if (Protocol && stricmp(Protocol,"icmp") == 0)
                {
                HintsV4.ai_socktype = HintsV6.ai_socktype = SOCK_RAW;
                HintsV4.ai_protocol = HintsV6.ai_protocol = IPPROTO_ICMP;
                }
        else if (Protocol && stricmp(Protocol,"esp") == 0)
                {
                HintsV4.ai_socktype = HintsV6.ai_socktype = SOCK_RAW;
                HintsV4.ai_protocol = HintsV6.ai_protocol = IPPROTO_ESP;
                }
        int rc = getaddrinfo(Address,           // Try requested family first.
                             Service,
                             AddressFamily == AF_INET ? &HintsV4 : &HintsV6,
                             &Results);
        if (rc == 0)
                {
  AddrInfoOK:   size_t MinLength = sizeof_sockaddr(*Results->ai_addr);
                size_t MaxLength = sizeof(*AddrDest);
                if (MinLength > MaxLength) MinLength = MaxLength;
                memcpy(AddrDest,Results->ai_addr,MinLength);
                freeaddrinfo(Results);
                }
        else    {
                rc = getaddrinfo(Address,       // Try alternate family.
                                 Service,
                                 AddressFamily == AF_INET ? &HintsV6 : &HintsV4,
                                 &Results);
                if (rc == 0) goto AddrInfoOK;
                }
        if (rc && LastError) *LastError = WSAGetLastError();
        return rc == 0;
        }

static flag ResolveAddress(socketCBaddr *AddrDest, int AddressFamily,
                                                   const char *Address, 
                                                   int Port, 
                                                   int *LastError)
        {
        flag Status;
        if (!AddrDest) return false;
        unsigned short netPort = htons((unsigned short)Port);
        memset(AddrDest,0,sizeof(socketCBaddr));
        if (!Address)
                {
                AddrDest->in.family = AddressFamily;
                AddrDest->in.port = netPort;
                Status = true;
                }
        else    {
                Status = ResolveAddress(AddrDest,AddressFamily,
                                                 Address,
                                                 NULL,
                                                 NULL,
                                                 LastError);
                if (Status) AddrDest->in.port = netPort;
                }
        return Status;
        }

#else /* SOCKETCB_IPV4 */

// **************************
// ********** IPv4 **********
// **************************

static flag ResolveHost(in_addr *AddrDest, const char *HostSrc, int *LastErr)
        {
        AddrDest->s_addr = inet_addr((char *)HostSrc);
        if (AddrDest->s_addr == INADDR_NONE)    // Not a numerical address.
                {                               // Resolve host name.
                hostent *HostAddr;
  #ifdef __GNUC__
                int Errno = 0;
                hostent _HostAddr;
                char Buffer[1024];
                int rc = gethostbyname_r((char *)HostSrc,
                                         &_HostAddr,
                                         Buffer,
                                         sizeof(Buffer),
                                         &HostAddr,
                                         &Errno);
                if (rc || HostAddr == NULL)
                        {
                        if (LastErr) *LastErr = Errno;
                        return false;
                        }
  #else
                HostAddr = gethostbyname((char *)HostSrc);      
                if (HostAddr == NULL)
                        {
                        if (LastErr) *LastErr = WSAGetLastError();
                        return false;
                        }
  #endif
                size_t MinLength = HostAddr->h_length;
                size_t MaxLength = sizeof(*AddrDest);
                if (MinLength > MaxLength) MinLength = MaxLength;
                memcpy(AddrDest,HostAddr->h_addr,MinLength);
                }
        return true;
        }

static flag ResolveAddress(socketCBaddr *AddrDest, int AddressFamily,
                                                   const char *Address, 
                                                   const char *Service,
                                                   const char *Protocol,
                                                   int *LastError)
        {
        flag Status;
        if (!AddrDest) return false;
        memset(AddrDest,0,sizeof(socketCBaddr));        // INADDR_ANY
        AddrDest->sin_family = AddressFamily;
        unsigned Port = atoi(Service);
        if (!Port && Service[0] != '0')
                {
                struct servent *ServiceEntry;
                ServiceEntry = getservbyname((char*)Service,(char*)Protocol);
                if (ServiceEntry)
                        {
                        Port = ntohs((short)ServiceEntry->s_port);
                        }
                }
        AddrDest->sin_port = htons((unsigned short)Port);
        if (Address) 
                {
                Status = ResolveHost(&AddrDest->sin_addr,Address,LastError);
                }
        else Status = true;
        return Status;
        }

static flag ResolveAddress(socketCBaddr *AddrDest, int AddressFamily,
                                                   const char *Address, 
                                                   int Port, 
                                                   int *LastError)
        {
        flag Status;
        if (!AddrDest) return false;
        memset(AddrDest,0,sizeof(socketCBaddr));        // INADDR_ANY
        AddrDest->sin_family = AddressFamily;
        AddrDest->sin_port = htons((unsigned short)Port);
        if (Address) 
                {
                Status = ResolveHost(&AddrDest->sin_addr,Address,LastError);
                }
        else Status = true;
        return Status;
        }

#endif /* SOCKETCB_IPV4 */

// ********************************
// ********** IPv4 /IPv6 **********
// ********************************

flag ResolveAddress(socketCBaddr *AddrDest, const char *Host, 
                                            const char *Service,
                                            const char *Protocol)
        {
        int LastError;
  #ifdef SOCKETCB_IPV6
        flag rc = ResolveAddress(AddrDest,
                                 !Host || !*Host        /* Hint */
                                  ? AF_INET6
                                  : inet_addr((char *)Host) != INADDR_NONE
                                    ? AF_INET
                                    : stricmp(Host,"localhost") == 0
                                      ? AF_INET         /* Try IPv4 first */
                                      : AF_INET6,
                                 Host,
                                 Service,
                                 Protocol,
                                 &LastError);
  #else
        flag rc = ResolveAddress(AddrDest,AF_INET,
                                          Host,
                                          Service,
                                          Protocol,
                                          &LastError);
  #endif
        return rc;
        }

flag ResolveAddress(socketCBaddr *AddrDest, const char *Host, int Port)
        {
        int LastError;
  #ifdef SOCKETCB_IPV6
        flag rc = ResolveAddress(AddrDest,
                                 !Host || !*Host        /* Hint */
                                  ? AF_INET6
                                  : inet_addr((char *)Host) != INADDR_NONE
                                    ? AF_INET
                                    : stricmp(Host,"localhost") == 0
                                      ? AF_INET         /* Try IPv4 first */
                                      : AF_INET6,
                                 Host,
                                 Port,
                                 &LastError);
  #else
        flag rc = ResolveAddress(AddrDest,AF_INET,Host,Port,&LastError);
  #endif
        return rc;
        }

// ****************************************************************************
// **** WinSock2 Protocol Enumerator ******************************************
// ****************************************************************************

#if USE_WINSOCK2

static flag FindProtocol(WSAPROTOCOL_INFO *ProtocolInfo,
                         int iAddressFamily,
                         int iProtocol, 
                         flag QOS)
        {
        int i, rc;
        DWORD InfoLength;
        flag Found = false;
        WSAPROTOCOL_INFO *Info;
        int WantsQOS = QOS ? XP1_QOS_SUPPORTED : 0;

        if (!ProtocolInfo) return false;
        rc = WSAEnumProtocols(NULL, NULL, &InfoLength);
        if (rc != SOCKET_ERROR) return false;   // No buffer -> should fail.
        Info = (WSAPROTOCOL_INFO *)malloc(InfoLength);
        if (!Info) return false;
        rc = WSAEnumProtocols(NULL, Info, &InfoLength);
        if (rc == SOCKET_ERROR) goto FP_Exit;
        for (i=0; i < rc; i++)
                {
                int HasQOS = XP1_QOS_SUPPORTED & Info[i].dwServiceFlags1;
                if (Info[i].iAddressFamily == iAddressFamily &&
                    Info[i].iProtocol == iProtocol &&
                    WantsQOS == HasQOS 
  #ifdef _NOAventail_Socket
                    && strstr(Info[i].szProtocol,"Aventail") == NULL
                    && strstr(Info[i].szProtocol,"Hummingbird") == NULL
                    && strstr(Info[i].szProtocol,"Socks for WinSock") == NULL
  #endif
                    ) 
                        {
                        memcpy(ProtocolInfo,&Info[i],sizeof(Info[i]));
                        Found = true;
                        break;
                        }
                }
 FP_Exit:
        free(Info);
        return Found;
        }

#endif /* USE_WINSOCK2 */

// ****************************************************************************
// **** Socket Control Block **************************************************
// ****************************************************************************

void SocketCtrlBlock::_Init(size_t _VerifyStruct)
        {
        static const char *ProcName = "SocketCtrlBlock::Init";
        if (_VerifyStruct != sizeof(SocketCtrlBlock))
                {
                TERROR(("%s: Invalid SocketCtrlBlock - Recompile",ProcName));
                exit(-1);
                }
        LastError = 0;
        Bound = false;
        Connected = false;
        ReuseAddress = false;
        SkipIPHeader = false;
        NonblockingClose = false;
        Socket = NULLSOCKET;
        SocketType = NOSOCKETTYPE;
        AddressFamily = NOADDRESSFAMILY;
        memset(&ClientAddr,0,sizeof(ClientAddr));
        memset(&LocalAddr,0,sizeof(LocalAddr));
        memset(&DeferSet,0,sizeof(DeferSet));
        InitializeSockets();
        return;
        }

SocketCtrlBlock::~SocketCtrlBlock()
        {
        if (IsValidSocket(Socket)) Close();
        DeinitializeSockets();
        return;
        }

flag SocketCtrlBlock::Accept(SocketCtrlBlock &NewCtrlBlock)
        {
        int rc = 0;
        socklen_t ClientAddrSize = sizeof(socketCBaddr);
        SOCKET &NewSocket = NewCtrlBlock.Socket;
        assert(IsNullSocket(NewSocket));
        while (true)
                {
                NewSocket = accept(Socket,
                                   (LPSOCKADDR)&NewCtrlBlock.ClientAddr,
                                   &ClientAddrSize);
                if (!IsValidSocket(NewSocket) && WSAGetLastError() == EINTR)
                        {
                        continue;
                        }
                break;
                }
        if (!IsValidSocket(NewSocket))
                {
                LastError = WSAGetLastError();
                return false;
                }
        socklen_t LocalAddrSize = sizeof(LocalAddr);
        LPSOCKADDR NewLocalAddr = (LPSOCKADDR)&NewCtrlBlock.LocalAddr;
        getsockname(NewSocket,NewLocalAddr,&LocalAddrSize);
        NewCtrlBlock.AddressFamily = AddressFamily;
        NewCtrlBlock.SocketType = SOCK_STREAM;
        NewCtrlBlock.Connected = true;
        NewCtrlBlock.Bound = true;
        return rc == 0;
        }

flag SocketCtrlBlock::AllowAddressReuse(flag allow)
        {
        if (!IsNullSocket(Socket)) return false;        // Must be set before
        ReuseAddress = allow;                           // binding socket.
        return true;
        }

flag SocketCtrlBlock::Bind()
        {
        static const char *ProcName = "SocketCtrlBlock::Bind";
        assert(!Bound);
        socketCBaddr_t *LocalCBaddr = (socketCBaddr_t *)&LocalAddr;
        if (!LocalCBaddr->in.family)
                {
                LocalCBaddr->in.family = AddressFamily;
                }
        if (LocalCBaddr->in.family == AF_INET6)            
                {                                       // Listen on both IPv6
                IPV6ONLY_t IPV6Only = 0;                // and IPv4 addresses.
                setsockopt(IPPROTO_IPV6,
                           IPV6_V6ONLY,
                           (char *)&IPV6Only,
                           sizeof(IPV6Only));
                }
        int rc = bind(Socket,
                      (LPSOCKADDR)&LocalAddr,
                      sizeof_sockaddr(LocalAddr));
        if (rc)
                {
                LastError = WSAGetLastError();
                TERROR(("%s: bind(%s) Failed (%d)",
                        ProcName,
                        SOCKADDR2String(&LocalAddr),
                        LastError));
                }
        else    {       
                socklen_t LocalAddrSize = sizeof(LocalAddr);
                getsockname(Socket,(LPSOCKADDR)&LocalAddr,&LocalAddrSize);
                }
        if (rc == 0) Bound = true;
        return Bound;
        }

flag SocketCtrlBlock::Close(flag Gracefull)
        {                                       // Default: Gracefull = false.
        int rc = 0;
        if (_IsDeferedOpen()) goto Exit;
        assert(IsValidSocket(Socket));
        if (!NonblockingClose || !Gracefull)
                {
                linger Linger;
                Linger.l_onoff = 1;
                Linger.l_linger = Gracefull ? CLOSE_LINGER_TIME : 0;
                setsockopt(SOL_SOCKET,SO_LINGER,(char*)&Linger,sizeof(linger));
                }
        else /* (NonblockingClose && Gracefull) */   // Call SetNonblocking()
                {                                    // before Close() to
                linger Linger;                       // avoid blocking close.
                Linger.l_onoff = 0;
                Linger.l_linger = 0;
                setsockopt(SOL_SOCKET,SO_LINGER,(char*)&Linger,sizeof(linger));
                }
        rc = closesocket(Socket);
        if (rc) LastError = WSAGetLastError();
  Exit: Bound = false;
        Connected = false;
        SkipIPHeader = false;
        Socket = NULLSOCKET;
        SocketType = NOSOCKETTYPE;
        AddressFamily = NOADDRESSFAMILY;
        memset(&ClientAddr,0,sizeof(ClientAddr));
        memset(&LocalAddr,0,sizeof(LocalAddr));
        memset(&DeferSet,0,sizeof(DeferSet));
        return rc == 0;
        }

flag SocketCtrlBlock::Connect(const socketCBaddr &Client)
        {
        static const char *ProcName = "SocketCtrlBlock::Connect";
        if (Connected) return false;
        if (!IsValidSocket(Socket))             // Handle defered open.
                {
                if (SocketType != SOCK_STREAM)
                        {
                        TERROR(("%s: Socket not opened for TCP",ProcName));
                        return false;
                        }
                else    {
                        int Family = ((socketCBaddr_t &)Client).in.family;
                        if (Family == NOADDRESSFAMILY)
                                {
                                TERROR(("%s: No Address Family",ProcName));
                                return false;
                                }
                        flag Status = OpenSocket(Family,
                                                 NULL,0,
                                                 SOCK_STREAM,
                                                 IPPROTO_TCP);
                        if (!Status)
                                {
                                TERROR(("%s: OpenSocket() Failed",ProcName));
                                return false;
                                }
                        }
                }
        size_t MinLength = sizeof_sockaddr(Client);
        size_t MaxLength = sizeof(ClientAddr);
        if (MinLength > MaxLength) MinLength = MaxLength;
        memcpy(&ClientAddr,&Client,MinLength);
        int rc = connect(Socket,(LPSOCKADDR)&Client,MinLength);
        if (rc) LastError = WSAGetLastError();
        else    {
                Bound = true;
                Connected = true;
                socklen_t LocalAddrSize = sizeof(LocalAddr);
                getsockname(Socket,(LPSOCKADDR)&LocalAddr,&LocalAddrSize);
                }
        return rc == 0;
        }

flag SocketCtrlBlock::Connect(const char *Address, int Port)
        {
        assert(Address);
        socketCBaddr Client;
        if (!ResolveAddress(&Client,Address,Port)) return false;
        return Connect(Client);
        }

void SocketCtrlBlock::GetBufferSizes(size_t *SendLength, size_t *RecvLength)
        {
        int BufferLength;
        socklen_t BufferLengthLength = sizeof(int);
        if (SendLength)
                {
                ::getsockopt(Socket,
                             SOL_SOCKET,
                             SO_SNDBUF,
                             (char *)&BufferLength,
                             &BufferLengthLength);
                *SendLength = (size_t)BufferLength;
                }
        if (RecvLength)
                {
                ::getsockopt(Socket,
                             SOL_SOCKET,
                             SO_RCVBUF,
                             (char *)&BufferLength,
                             &BufferLengthLength);
                *RecvLength = (size_t)BufferLength;
                }
        return;
        }

flag SocketCtrlBlock::GetNoDelay()
        {
        TCPNODELAY_t TCPNoDelay;
        socklen_t TCPNoDelayLength = sizeof(TCPNoDelay);
        ::getsockopt(Socket,
                     IPPROTO_TCP,
                     TCP_NODELAY,
                     (char *)&TCPNoDelay,
                     &TCPNoDelayLength);
        return TCPNoDelay ? true : false;
        }

SOCKET SocketCtrlBlock::GetSocketHandle() 
        {
        static const char *ProcName = "SocketCtrlBlock::GetSocketHandle";
        if (_IsDeferedOpen())
                {
                TDEBUG(("%s: Was Defered : Open(AF_INET)",ProcName));
                flag Status = OpenSocket(AF_INET,       // If AF_INET6,
                                         NULL,0,        // then call
                                         SOCK_STREAM,   // OpenTCPV6(),
                                         IPPROTO_TCP);  // OpenTCP(::0).
                if (!Status)
                        {
                        TERROR(("%s: OpenSocket() Failed",ProcName));
                        }
                }
        return Socket;
        }

flag SocketCtrlBlock::_IsDeferedOpen() 
        {
        return !IsValidSocket(Socket) && SocketType == SOCK_STREAM;
        }

flag SocketCtrlBlock::IsReadyToRead(unsigned TimeoutSEC, unsigned TimeoutUSEC)
        {
        fd_set Sockets;
        timeval Timeout;
        int SelectResult;
        if (Connected) return false;
        FD_ZERO(&Sockets);
        FD_SET(Socket,&Sockets);
        Timeout.tv_sec = TimeoutSEC;
        Timeout.tv_usec = TimeoutUSEC;
        SelectResult = select(1,&Sockets,NULL,NULL,&Timeout);
        return SelectResult == 1;
        }

flag SocketCtrlBlock::Listen()
        {
        static const char *ProcName = "SocketCtrlBlock::Listen";
        int rc;
        if (!IsValidSocket(Socket))             // Handle defered open.
                {
                if (SocketType != SOCK_STREAM)
                        {
                        TERROR(("%s: Socket not opened for TCP",ProcName));
                        return false;
                        }
                else    {                                     // If AF_INET6,
                        flag Status = OpenSocket(AF_INET,     // then call
                                                 NULL,0,      // OpenTCPV6(),
                                                 SOCK_STREAM, // OpenTCP(::0).
                                                 IPPROTO_TCP);
                        if (!Status)
                                {
                                TERROR(("%s: OpenSocket() Failed",ProcName));
                                return false;
                                }
                        }
                }
        if (!Bound && !Bind()) 
                {
                TERROR(("%s: Bind() Failed",ProcName));
                return false;
                }
        rc = listen(Socket,LISTEN_BACKLOG_SIZE);
        if (rc) LastError = WSAGetLastError();
        return rc == 0;
        }

flag SocketCtrlBlock::OpenSocket(int Family,
                                 const char *Address, 
                                 int Port, 
                                 int Type, 
                                 int Proto)
        {
        static const char *ProcName = "SocketCtrlBlock::OpenSocket";
        flag Status;
        assert(IsNullSocket(Socket));
        // ******************************************************
        // **** Resolve Local Interface (and Address Family) ****
        // ******************************************************
        if ((Address && *Address) || Port)
                {
                bool Status = ResolveAddress(&LocalAddr,Address,Port);
                if (Status)
                        {
                        Family = ((socketCBaddr_t &)LocalAddr).in.family;
                        }
                else    {
                        TERROR(("%s: ResolveAddress(%s:%d) Failed",
                                ProcName,
                                Address && *Address ? Address : "*",
                                Port));
                        SocketType = NOSOCKETTYPE;
                        return false;
                        }
                }
        if (Family == NOADDRESSFAMILY)  // Don't know family yet.
                {
                TDEBUG(("%s: Defering TCP Open",ProcName));
                SocketType = Type;
                return true;
                }
        // ***********************
        // **** Create Socket ****
        // ***********************

  #if USE_WINSOCK2

        WSAPROTOCOL_INFO ProtocolInfo;
        if (FindProtocol(&ProtocolInfo,Family,Proto,false))
                {
                Socket = WSASocket(FROM_PROTOCOL_INFO,
                                   FROM_PROTOCOL_INFO,
                                   FROM_PROTOCOL_INFO,
                                   &ProtocolInfo, 0, WSA_FLAG_OVERLAPPED);
                }
        else if (Type == SOCK_RAW)
                
                {                               // Find doesn't seem to find
                Socket = WSASocket(Family,      // raw protocols (e.g. ICMP).
                                   Type,
                                   Proto,
                                   NULL, 0, WSA_FLAG_OVERLAPPED);
                }
        else    {
                TERROR(("%s: No Protcol For Socket (%d,%d,%d)",ProcName,
                                                               Family,
                                                               Type,
                                                               Proto));
                LastError = WSAEPROTONOSUPPORT;
                SocketType = NOSOCKETTYPE;
                return false;
                }

  #else /* !USE_WINSOCK2 */

        Socket = socket(Family,Type,Proto);

  #endif /* !USE_WINSOCK2 */
        
        if (IsNullSocket(Socket)) 
                {
                TERROR(("%s: Create Socket(%d,%d,%d) Failed",ProcName,
                                                             Family,
                                                             Type,
                                                             Proto));
                SocketType = NOSOCKETTYPE;
                return false;
                }
        SocketType = Type;
        AddressFamily = Family;
        // ******************************************
        // **** Conditionaly Allow Address Reuse ****
        // ******************************************
        if (ReuseAddress)                       // Indicate that the socket
                {                               // may bind to an address
                REUSEADDR_t value = 1;          // that is already in use.
                char *option = (char *)&value;
                setsockopt(SOL_SOCKET,SO_REUSEADDR,option,sizeof(value));
                }
        // *****************************
        // **** Set Defered Options ****
        // *****************************
        if (DeferSet.SetBlocking) SetBlocking();
        if (DeferSet.SetNonblocking) SetNonblocking();
        if (DeferSet.SetBufferSendSize && DeferSet.SetBufferRecvSize) 
                {
                size_t SendLength = DeferSet.SetBufferSendSize;
                size_t RecvLength = DeferSet.SetBufferRecvSize;
                SetBufferSizes(SendLength,RecvLength);
                }
        if (DeferSet.SetKeepAlive) SetKeepAlive(true);
        if (DeferSet.SetNoDelay) SetNoDelay(true);
        // *******************************
        // **** Bind To Local Address ****
        // *******************************
        if (Address && *Address)
                {
                Status = Bind();        // Address specified: bind requested.
                if (!Status)
                        {
                        TERROR(("%s: Bind(%s:%d) Failed",
                                ProcName,
                                Address && *Address ? Address : "*",
                                Port));
                        }
                }
        else Status = true;             // Delayed bind.
        return Status;
        }

flag SocketCtrlBlock::OpenRAWsocket(const char *Address, int Protocol)
        {
        static const char *ProcName = "SocketCtrlBlock::OpenRAWsocket";
        flag Status = OpenSocket(AF_INET,Address,0,SOCK_RAW,Protocol);
        if (Status && !Bound)
                {
                Status = Bind();
                if (!Status) TERROR(("%s: Bind() Failed",ProcName));
                }
        return Status;
        }

flag SocketCtrlBlock::OpenTCPsocket(const char *Address, int Port)
        {
        static const char *ProcName = "SocketCtrlBlock::OpenTCPsocket";
        flag rc;
        rc = OpenSocket(NOADDRESSFAMILY,Address,Port,SOCK_STREAM,IPPROTO_TCP);
        return rc;
        }

flag SocketCtrlBlock::OpenUDPsocket(const char *Address, int Port)
        {
        static const char *ProcName = "SocketCtrlBlock::OpenUDPsocket";
        flag Status = OpenSocket(AF_INET,Address,Port,SOCK_DGRAM,IPPROTO_UDP);
        if (Status && !Bound)
                {
                Status = Bind();
                if (!Status) TERROR(("%s: Bind() Failed",ProcName));
                }
        return Status;
        }

flag SocketCtrlBlock::OpenTCPsocketV6(const char *Address, int Port)
        {
        return OpenSocket(AF_INET6,Address,Port,SOCK_STREAM,IPPROTO_TCP);
        }

flag SocketCtrlBlock::OpenUDPsocketV6(const char *Addr, int Port)
        {
        static const char *ProcName = "SocketCtrlBlock::OpenUDPsocketV6";
        flag Status = OpenSocket(AF_INET6,Addr,Port,SOCK_DGRAM,IPPROTO_UDP);
        if (Status && !Bound)
                {
                Status = Bind();
                if (!Status) TERROR(("%s: Bind() Failed",ProcName));
                }
        return Status;
        }

int SocketCtrlBlock::Peek(char *Buffer, int Size)
        {
        socketCBaddr Addr;
        return PeekFrom(&Addr,Buffer,Size);
        }

int SocketCtrlBlock::PeekFrom(socketCBaddr *Addr, char *Buffer, int Size)
        {
        int Length;
        socklen_t AddrSize = sizeof(socketCBaddr);
        while (true)
                {
                Length = recvfrom(Socket,Buffer,Size,
                                  MSG_PEEK,(LPSOCKADDR)Addr,&AddrSize);
                if (Length == SOCKET_ERROR && WSAGetLastError() == EINTR)
                        {
                        continue;
                        }
                break;
                }
        if (Length == SOCKET_ERROR) LastError = WSAGetLastError();
        return Length;
        }

int SocketCtrlBlock::Receive(char *Buffer, int Size)
        {
#ifdef __WIN32__
        socketCBaddr Addr;
        socklen_t AddrSize = sizeof(socketCBaddr);
        int Length = recvfrom(Socket,Buffer,Size,0,(LPSOCKADDR)&Addr,&AddrSize);
#else
        int Length;
        while (true)
                {
                Length = recv(Socket,Buffer,Size,0);
                if (Length == SOCKET_ERROR && WSAGetLastError() == EINTR)
                        {
                        continue;
                        }
                break;
                }
#endif
        if (Length == SOCKET_ERROR) LastError = WSAGetLastError();
        return Length;
        }

int SocketCtrlBlock::ReceiveFrom(socketCBaddr *Addr, char *Buff, int Size)
        {
        int Length;
        socklen_t AddrSize = sizeof(socketCBaddr);
        while (true)
                {
                Length = recvfrom(Socket,Buff,Size,0,(LPSOCKADDR)Addr,&AddrSize);
                if (Length == SOCKET_ERROR && WSAGetLastError() == EINTR)
                        {
                        continue;
                        }
                break;
                }
        if (Length == SOCKET_ERROR) LastError = WSAGetLastError();
        return Length;
        }

flag SocketCtrlBlock::ResolveAddress(socketCBaddr *Dest, 
                                     const char *Host, 
                                     int Port)
        {
        flag rc = ::ResolveAddress(Dest,
                                   AddressFamily != NOADDRESSFAMILY
                                   ? AddressFamily
  #ifdef SOCKETCB_IPV6
                                   : !Host || !*Host
                                     ? AF_INET6
                                     : inet_addr((char *)Host) != INADDR_NONE
                                       ? AF_INET
                                       : stricmp(Host,"localhost") == 0
                                         ? AF_INET      /* Try IPv4 first */
                                         : AF_INET6,
  #else
                                   : AF_INET,
  #endif
                                   Host,
                                   Port,
                                   &LastError);
        return rc;
        }

int SocketCtrlBlock::Send(const char *Buffer, int Size)
        {
        int Length;
        while (true)
                {
                Length = send(Socket,(char *)Buffer,Size,0);
                if (Length == SOCKET_ERROR && WSAGetLastError() == EINTR)
                        {
                        continue;
                        }
                break;
                }
        if (Length == SOCKET_ERROR) LastError = WSAGetLastError();
        return Length;
        }

int SocketCtrlBlock::SendTo(const char *Addr, 
                            int Port, 
                            const char *Buffer,
                            int Size)
        {
        socketCBaddr Address;
        if (!ResolveAddress(&Address,Addr,Port)) return SOCKET_ERROR;
        return SendTo(Address,Buffer,Size);
        }

#define SkipIPv4HeaderLEN (sizeof(IPv4Header) - 1)

int SocketCtrlBlock::SendTo(const socketCBaddr &Address, 
                            const char *Buffer, 
                            int Size)
        {
        int Length;
        while (true)
                {
                if (SkipIPHeader)               // For SOCK_RAW sockets.
                        {
                        assert(Size >= SkipIPv4HeaderLEN);
                        Buffer += SkipIPv4HeaderLEN;
                        Size -= SkipIPv4HeaderLEN;
                        }
                Length = sendto(Socket,(char *)Buffer,Size,0,
                                (LPSOCKADDR)&Address,
                                sizeof_sockaddr(Address));
                if (Length == SOCKET_ERROR)
                        {
                        if (WSAGetLastError() == EINTR) continue;
                        }
                else if (SkipIPHeader) Length += SkipIPv4HeaderLEN;
                break;
                }
        if (Length == SOCKET_ERROR) LastError = WSAGetLastError();
        return Length;
        }

flag SocketCtrlBlock::SetBlocking()
        {
        int rc;
        if (_IsDeferedOpen())
                {
                DeferSet.SetBlocking = true;
                return true;
                }
        #ifdef __WIN32__
                u_long NonBlockingOption = 0;
                rc = ioctlsocket(Socket,FIONBIO,&NonBlockingOption);
        #elif defined(FNDELAY)
                int flags = fcntl(Socket,F_GETFL,FNDELAY);
                if (flags == -1) flags = 0;
                flags = (int)((unsigned)flags & ~(unsigned)FNDELAY);
                rc = fcntl(Socket,F_SETFL,flags);
        #else
                int NonBlockingOption = 0;
                rc = ioctl(Socket,FIONBIO,&NonBlockingOption);
        #endif
        return rc == 0;
        }

void SocketCtrlBlock::SetBufferSizes(size_t SendLength, size_t RecvLength)
        {
        if (_IsDeferedOpen())
                {
                DeferSet.SetBufferSendSize = SendLength;
                DeferSet.SetBufferRecvSize = RecvLength;
                return;
                }
        int BufferLength;
        BufferLength = (int)SendLength;
        ::setsockopt(Socket,
                     SOL_SOCKET,
                     SO_SNDBUF,
                     (char *)&BufferLength,
                     sizeof(BufferLength));
        BufferLength = (int)RecvLength;
        ::setsockopt(Socket,
                     SOL_SOCKET,
                     SO_RCVBUF,
                     (char *)&BufferLength,
                     sizeof(BufferLength));
        return;
        }

void SocketCtrlBlock::SetHeaderCtrl(flag SkipHeader, flag IncludeHeader)
        {
  #if defined(__OS400__) || !USE_WINSOCK2
        SkipIPHeader = SkipHeader || IncludeHeader; // No option, always skip.
  #else
        int Incl = IncludeHeader ? 1 : 0;
        setsockopt(IPPROTO_IP,IP_HDRINCL,(char*)&Incl,sizeof(Incl));
        if (!IncludeHeader) SkipIPHeader = SkipHeader;
        else SkipIPHeader = false;
  #endif
        return;
        }

void SocketCtrlBlock::SetKeepAlive(flag KeepAlive)
        {
        if (_IsDeferedOpen())
                {
                DeferSet.SetKeepAlive = KeepAlive;
                return;
                }
        SOKEEPALIVE_t SOKeepAlive = KeepAlive ? 1 : 0;
        ::setsockopt(Socket,
                     SOL_SOCKET,
                     SO_KEEPALIVE,
                     (char *)&SOKeepAlive,
                     sizeof(SOKeepAlive));
        return;
        }

void SocketCtrlBlock::SetNoDelay(flag NoDelay)
        {
        if (_IsDeferedOpen())
                {
                DeferSet.SetNoDelay = NoDelay;
                return;
                }
        TCPNODELAY_t TCPNoDelay = NoDelay ? 1 : 0;
        ::setsockopt(Socket,
                     IPPROTO_TCP,
                     TCP_NODELAY,
                     (char *)&TCPNoDelay,
                     sizeof(TCPNoDelay));
        return;
        }

flag SocketCtrlBlock::SetNonblocking()
        {
        int rc;
        if (_IsDeferedOpen())
                {
                DeferSet.SetNonblocking = true;
                return true;
                }
        #ifdef __WIN32__
                u_long NonBlockingOption = 1;
                rc = ioctlsocket(Socket,FIONBIO,&NonBlockingOption);
        #elif defined(FNDELAY)
                int flags = fcntl(Socket,F_GETFL,FNDELAY);
                if (flags == -1) flags = 0;
                rc = fcntl(Socket,F_SETFL,flags | FNDELAY);
        #else
                int NonBlockingOption = 1;
                rc = ioctl(Socket,FIONBIO,&NonBlockingOption);
        #endif
        NonblockingClose = rc == 0;
        return rc == 0;
        }

void SocketCtrlBlock::SetNonblockingClose()
        {
        NonblockingClose = true;
        return;
        }

flag SocketCtrlBlock::_SetSocketHandle(SOCKET Handle) 
        {
        Socket = Handle;
        if (Socket == INVALID_SOCKET)
                {
                memset(&ClientAddr,0,sizeof(ClientAddr));
                memset(&LocalAddr,0,sizeof(LocalAddr));
                memset(&DeferSet,0,sizeof(DeferSet));
                return true;
                }
        socklen_t LocalAddrSize = sizeof(LocalAddr);
        int rc = getsockname(Socket,(LPSOCKADDR)&LocalAddr,&LocalAddrSize);
        if (rc) 
                {
                LastError = WSAGetLastError();
                return false;
                }
        socklen_t ClientAddrSize = sizeof(ClientAddr);
        rc = getpeername(Socket,(LPSOCKADDR)&ClientAddr,&ClientAddrSize);
        if (rc) 
                {
                LastError = WSAGetLastError();
                return false;
                }
        memset(&DeferSet,0,sizeof(DeferSet));
        return true;
        }

int SocketCtrlBlock::setsockopt(int level, 
                                int optname, 
                                char *optval, 
                                int optlen)
        {
        assert(IsValidSocket(Socket));
        return ::setsockopt(Socket,level,optname,optval,optlen);
        }

flag SocketCtrlBlock::Shutdown()
        {
        if (_IsDeferedOpen()) return true;
        assert(IsValidSocket(Socket));
        int rc = shutdown(Socket,SD_BOTH);
        return rc == 0;
        }

// ****************************************************************************
// **** IPv4 RAW Socket Utilities *********************************************
// ********************************
//
//              CalcChecksum() - One complement 16 bit checksum.
//                               skipNth - Assumes the value of this short
//                                         is zero (Use to ignore the 
//                                         checksum in the buffer itself).
//                                         skipNth=0 ==> Nothing to ignore.
//
// ****************************************************************************

unsigned short CalcChecksum(const void *Buffer, int nShorts, int skipNth)
        {
        unsigned short Checksum = 0;
        unsigned short *BufferPos = (unsigned short *)Buffer;
        for (int i=0; i < nShorts; i++)
                {                               // Invariant to Big- or
                if (i+1 != skipNth)             // Little- Endian buffers.
                        {
                        unsigned Sum = Checksum + *BufferPos;
                        unsigned Carry = Sum >> 16;
                        Checksum = (unsigned short)(Sum + Carry);
                        }
                BufferPos++;
                }
        return (unsigned short)~Checksum;
        }

unsigned short IPv4Checksum(const IPv4Header *Header)
        {
        unsigned nShorts = IPv4Header_IHL(Header) * 2;
        return CalcChecksum(Header,nShorts,6);
        }

// ****************************************************************************
// ****************************** End of File *********************************
// ****************************************************************************
