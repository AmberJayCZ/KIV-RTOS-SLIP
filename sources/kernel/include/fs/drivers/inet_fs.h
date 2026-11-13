#pragma once

#include <drivers/bridges/inet_defs.h>
#include <drivers/inet.h>
#include <fs/filesystem.h>
#include <stdmutex.h>

constexpr uint32_t INET_SOCKET_ERROR = 0xFFFFFFFF; // IFile calls on a TINet_Socket that return this value were erroneous
constexpr uint32_t INET_MAX_REGISTERED_SOCKETS = 10; // Limit of sockets that can be registered in sINet_Socket_Provider

constexpr const char* INET_IP4 = "4"; // IPv4 string ID
constexpr const char* INET_UDP = "UDP"; // UDP string ID

struct _INet_TP_Container {
	const char* name;
	NTransport_Protocol proto;
};

const _INet_TP_Container transport_protos[] = {
	{ INET_UDP, NTransport_Protocol::UDP }
};

// Network socket status flags
enum class NINet_Socket_Flags : uint8_t {
	CLOSED 		= 1,	// Socket is closed
	CONFIGURED	= 2,	// Socket is configured
	BOUND 		= 4,	// Socket is bound to local interface
	CONNECTED	= 8,	// Socket is connected to remote host

	// IN_FULL 	= 32,	// Input buffer is full - TODO review if needed
	// OUT_FULL 	= 64,	// Output buffer is full - TODO review if needed
	ERROR		= 128,	// Last operation caused an error
};

// Network socket error codes
enum class NINet_Socket_Errors : uint32_t {
	OK = 0,				// No errors
	CONFIGURE_FAIL,		// Configuration of socket failed
	BIND_FAIL,			// Binding to local interface failed
	CONNECT_FAIL,		// Connecting to remote endpoint failed
	SEND_BUFF_OVERFLOW,	// No write was performed - not enough space in outbound buffer - TODO review if needed
	RECV_BUFF_OVERFLOW,	// Inbound buffer overflowed - TODO review if needed
	REPEATED_CALL		// This method was already called once and cannot be called again
};

// Network socket class
class CINet_Socket : public IFile {
	protected:
		NInternet_Protocol ip;
		NTransport_Protocol tp;
		NFile_Open_Mode mode;
		TIP_Address_Container local_addr, remote_addr; // Container for arbitrary raw data, used with reinterpret_cast and instance data to ascertain actual address type
		uint8_t flags;

		bool Compare_Incoming_Source(const TIP_Address_Container& in_src) const;

		bool Compare_Incoming_Destination(const TIP_Address_Container& in_dst) const;

	public:
		CINet_Socket(NFile_Open_Mode mode);

		// virtual ~CINet_Socket();

		// Sets the socket protocols. Socket configuration is permanent - once configured, the protocols cannot be changed
		virtual NINet_Socket_Errors Configure(const NInternet_Protocol internet, const NTransport_Protocol transport);

		// Binds socket to local address. Binding is permanent - once bound, local address cannot be changed
		virtual NINet_Socket_Errors Bind(const TIP_Address_Container& local);

		// Connects socket to remote address. Connecting is not permanent - once connected, the socket can be re-connected at will
		virtual NINet_Socket_Errors Connect(const TIP_Address_Container& remote);

		// Sends num bytes from buff. Requires connected socket, returns number of bytes sent or  INET_SOCKET_ERROR on fail
		virtual uint32_t Send(const char* buff, const uint32_t num);

		// Sends num bytes from buff to addr. Returns INET_SOCKET_ERROR on fail
		virtual uint32_t Send_To(const char* buff, const uint32_t num, TIP_Address_Container& addr);

		// Recieves at most num bytes into buff. Requires connected socket, returns number of bytes recieved or INET_SOCKET_ERROR on fail
		virtual uint32_t Recv(char* buff, const uint32_t num);

		// Recieves at most num bytes from addr into buff. Returns number of bytes recieved or INET_SOCKET_ERROR on fail
		virtual uint32_t Recv_From(char* buff, const uint32_t num, TIP_Address_Container& addr);

		// Returns bitmap of NINet_Socket_Flags that are set in this socket
		uint8_t Flags() const;

		// Tests if addr and tpv correspond to local bound address
		bool Compare_Local_Bind(const TIP_Address_Container& addr, const NTransport_Protocol tpv) const;

		// IFile implementation - all methods for future reference - TODO review which to implement / override

		virtual uint32_t Read(char* buffer, uint32_t num) override;

		// Sends num bytes from buff to remote address. Requires connected socket, returns INET_SOCKET_ERROR on fail
		virtual uint32_t Write(const char* buffer, uint32_t num) override;

		// Closes socket and un-registers it from control structs
		virtual bool Close() override;

		// virtual bool IOCtl(NIOCtl_Operation dir, void* ctlptr);

		// virtual bool Wait(uint32_t count);

		// virtual bool Try_Wait_All_Reserve(uint32_t count);

		// virtual bool Wait_All_Acquire(uint32_t count);

		// virtual uint32_t Notify(uint32_t count);

		// virtual void Wait_Enqueue_Current();

		// virtual bool Wait_Dequeue_Process(uint32_t pid);
};



class CINet_Socket_Provider {
	private:
		CINet_Socket* bound[INET_MAX_REGISTERED_SOCKETS];
		uint32_t sock_count;
		mutex_t provider_mtx;

	public:
		CINet_Socket_Provider();

		~CINet_Socket_Provider();

		// Register socket in provider. Returns false if registration failed
		bool Register_Socket(CINet_Socket* socket);

		// Un-register socket_addr from provider. Returns false if socket_addr is not in provider
		bool Unregister_Socket(CINet_Socket* socket_addr);

		// Tests if socket is contained in provider
		bool Contains(const CINet_Socket* socket) const;

		// Gets socket by address and tp. Returns nullptr if socket not found in bound socket array
		CINet_Socket* Get_Socket(const TIP_Address_Container& address, const NTransport_Protocol tp) const;
};

// Socket provider singleton
extern CINet_Socket_Provider sINet_Socket_Provider;



// Parses IP version into ip from str. Parameter end holds the index of str where parsing was stopped. Returns true if parsing was successful
bool Parse_IP(const char* str, uint32_t& end, NInternet_Protocol& ip);

// Parses TP version into tp from str. Parameter end holds the index of str where parsing was stopped. Returns true if parsing was successful
bool Parse_TP(const char* str, uint32_t& end, NTransport_Protocol& tp);

// Parses port into c from str. Parameter end holds the index of str where parsing was stopped. Returns true if parsing was successful
bool Parse_Port(const char* str, uint32_t& end, TIP_Address_Container& c);

// Parses IP address into c from str. Parameter end holds the index of str where parsing was stopped. Returns true if parsing was successful
bool Parse_Addr(const char* str, uint32_t& end, TIP_Address_Container& c, const NInternet_Protocol ip);