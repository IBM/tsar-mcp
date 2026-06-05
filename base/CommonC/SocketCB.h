// Socket Control Block: SocketCB.h
/*
 * TSAR (Tools Slightly Above the Runtime)
 * Filename: SocketCB.h
 *
 * Copyright (c) 2026 International Business Machines Corporation
 * Copyright (c) 1995 Eric Kass
 *
 * SPDX-License-Identifier: MIT
 */
//
//
//
//      Defined Macros          Values                  Default
//      --------------          ------                  -------
//
//      USE_WINSOCK2            0/1                     0
//
//      _NOAventail_Socket      defined/undefined       undefined
//      
//
//      Note: if _NOAventail_Socket is defined USE_WINSOCK2 is 1
// 

#ifndef _Network_Trasmission

	#define _Network_Trasmission

#if !defined(__WIN32__) && defined(_WIN32)
	#define __WIN32__
#endif

typedef bool flag;

#ifdef __WIN32__

        #include <winsock.h>

#else /* __UNIX__ */

        #include <errno.h>
	#include <sys/socket.h>
	#include <sys/types.h>
	#include <netinet/in.h>
        #include <netinet/tcp.h>

	#define SOCKET int
	#define SOCKET_ERROR ((int)-1)
	#define INVALID_SOCKET ((int)-1)
        #define WSAGetLastError() errno 
        #define WSAENOTSOCK EBADF
        #define WSAECONNREFUSED ECONNREFUSED
        #define WSAETIMEDOUT ETIMEDOUT
        
        #define closesocket close
#endif

#ifdef _WIN32
        #ifndef AF_INET6
                #define AF_INET6 23
        #endif
#endif

#ifdef _WIN32
        #pragma pack(push, 1)
#else
        #pragma pack(1)
#endif

struct socketCBaddr_t
        {
        union   {
                struct  {
  #ifdef _AIX
                        uchar_t len;
                        uchar_t family;
  #else
                        short family;
  #endif
                        u_short port;
                        } in;
                sockaddr_in in4;
                struct  {
  #ifdef _AIX
                        uchar_t len;
                        uchar_t family;
  #else
                        short family;
  #endif
                        u_short port;
                        unsigned int flowinfo;
                        struct  {
                                union   {
                                        u_char Byte[16];
                                        u_short Word[8];
                                        } u;
                                } in6_addr;
                        unsigned int scope_id;
                        } in6;
                unsigned char pad[64];
                };
        };

#define sizeof_sockaddr(addr)                                               \
        (((socketCBaddr_t&)(addr)).in.family == AF_INET                     \
         ? sizeof(sockaddr_in)                                              \
         : ((socketCBaddr_t&)(addr)).in.family == AF_INET6                  \
         ? sizeof(((socketCBaddr_t *)0)->in6)                               \
         : sizeof(sockaddr_in))

#ifdef __GNUC__
        #pragma pack()
#else
        #pragma pack(pop)
#endif

#ifdef SOCKETCB_IPV6
	typedef socketCBaddr_t socketCBaddr;
#else
        typedef sockaddr_in socketCBaddr;
#endif

#define NOSOCKETTYPE 0
#define NOADDRESSFAMILY 0
#define CLOSE_LINGER_TIME 5	  		// Seconds before socket close.
#define LISTEN_BACKLOG_SIZE 128            	// Maximum pending connections.
#define NULLSOCKET INVALID_SOCKET
#define IsNullSocket(socket) (socket == INVALID_SOCKET)
#define IsValidSocket(socket) (socket != INVALID_SOCKET)

// ****************************************************************************
// **** Socket Initialization *************************************************
// ****************************************************************************

bool InitializeSockets();

void DeinitializeSockets();

// ****************************************************************************
// **** Address utilities *****************************************************
// ****************************************************************************

flag ResolveAddress(socketCBaddr *AddrDest, const char *Host, int Port);

flag ResolveAddress(socketCBaddr *AddrDest, const char *Host, 
                                            const char *Service,
                                            const char *Protocol);

// ****************************************************************************
// **** Socket Control Block **************************************************
// ****************************************************************************
                                            
class SocketCtrlBlock
	{
	private:
                flag Bound;
		flag Connected;
                flag ReuseAddress;
                flag SkipIPHeader;
                flag NonblockingClose;
		socketCBaddr ClientAddr;
		socketCBaddr LocalAddr;
                struct _DeferSet
                        {
                        flag SetBlocking;
                        flag SetNonblocking;
                        size_t SetBufferSendSize;
                        size_t SetBufferRecvSize;
                        flag SetKeepAlive;
                        flag SetNoDelay;
                        } DeferSet;
                int LastError;
		SOCKET Socket;
		int SocketType;
                int AddressFamily;
		flag OpenSocket(int AddressFamily,
                                const char *Address, 
                                int Port, 
                                int Type, 
                                int Proto);
                void _Init(size_t _VerifyStruct);
	protected:
                flag _IsDeferedOpen();
                flag Bind();
                flag ResolveAddress(socketCBaddr *Dest, 
                                    const char *Host, 
                                    int Port);
		void SetLastError(int Error) {LastError = Error;}
	public:
                SocketCtrlBlock() {_Init(sizeof(SocketCtrlBlock));}
		~SocketCtrlBlock();
                flag AllowAddressReuse(flag allow);
		flag Close(flag Gracefull = false);
                void GetBufferSizes(size_t *SendLength, size_t *RecvLength);
		socketCBaddr* GetClientAddress() {return &ClientAddr;}
		int GetLastError() {return LastError;}
		socketCBaddr* GetLocalAddress() {return &LocalAddr;}
                flag GetNoDelay();
		SOCKET GetSocketHandle();
		flag IsDatagramSocket() {return SocketType == SOCK_DGRAM;}
		flag IsOpened();
		flag IsRawSocket() {return SocketType == SOCK_RAW;}
                flag IsReadyToRead(unsigned TimeoutSEC, unsigned TimeoutUSEC);
                flag IsStreamSocket() {return SocketType == SOCK_STREAM;}
		flag OpenRAWsocket(const char *LocalAddress, int Protocol);
		flag OpenTCPsocket(const char *LocalAddress, int Port);
		flag OpenUDPsocket(const char *LocalAddress, int Port);
                flag OpenTCPsocketV6(const char *LocalAddress, int Port);
		flag OpenUDPsocketV6(const char *LocalAddress, int Port);
                flag SetBlocking();
                void SetBufferSizes(size_t SendLength, size_t RecvLength);
                void SetHeaderCtrl(flag SkipHeader, flag IncludeHeader);
                void SetKeepAlive(flag KeepAlive);
                void SetNoDelay(flag NoDelay);
		flag SetNonblocking();
                void SetNonblockingClose();
                void _SetLastError(int Error) {LastError = Error;}
                flag _SetSocketHandle(SOCKET Handle);
		int setsockopt(int level,int optname,char *optval,int optlen);
                flag Shutdown();
		// ************************
		// **** Action Methods ****
		// ************************
		flag Listen();
		flag Accept(SocketCtrlBlock &NewSocket);
		flag Connect(const socketCBaddr &ClientAddress);
		flag Connect(const char *ClientAddress, int Port);
		int Peek(char *Buffer, int Size);
		int PeekFrom(socketCBaddr *Addr, char *Buffer, int Size);
		int Receive(char *Buffer, int Size);
		int ReceiveFrom(socketCBaddr *ClientAddr, char *Buff, int Size);
		int Send(const char *Buffer, int Size);
		int SendTo(const char *Address, 
                           int Port, 
                           const char *Buffer, 
                           int Size);
		int SendTo(const socketCBaddr &ClientAddr, 
                           const char *Buffer, 
                           int Size);
	};

inline flag SocketCtrlBlock::IsOpened() 
        {
        return IsValidSocket(Socket) || SocketType == SOCK_STREAM;
        }

// ****************************************************************************
// **** IPv4 RAW Socket Utilities *********************************************
// ****************************************************************************

#ifdef _WIN32
        #pragma pack(push, 1)
#else
        #pragma pack(1)
#endif

struct IPv4Header
        {
        unsigned char Version_IHL;      // Version : Header Length (n*32 bits)
        unsigned char TOS;              // Type of Service
        unsigned short TotalLength;
        unsigned short Identification;
        unsigned short Flags_Fragment;
        unsigned char TTL;
        unsigned char Protocol;
        unsigned short Checksum;        // Ones-Complement
        unsigned int SourceIP;
        unsigned int DestIP;
        unsigned char Payload[1];
        };

#define IPHeader_Version(Header) (((IPv4Header *)(Header))->Version_IHL >> 4)
#define IPv4Header_IHL(Header) ((Header)->Version_IHL & 0x0F)

struct ICMPEchoHeader
        {
        unsigned char Type;             // ICMP Type = 0 (Zero) == Echo
        unsigned char Code;             // Zero for Echo
        unsigned short Checksum;
        unsigned short Identification;  // May be zero
        unsigned short SequenceNumber;  // May be zero
        unsigned char Payload[1];
        };

#ifdef __GNUC__
        #pragma pack()
#else
        #pragma pack(pop)
#endif

unsigned short CalcChecksum(const void *Buffer, int nShorts, int skipNth);

unsigned short IPv4Checksum(const IPv4Header *Header);

// ****************************************************************************
// ****************************** End of File *********************************
// ****************************************************************************

#endif
