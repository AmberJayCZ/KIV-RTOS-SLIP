#pragma once

#include <drivers/bridges/inet_defs.h>
#include <drivers/uart.h>
#include <fs/filesystem.h>

constexpr auto INET_SOCKET_BUFFER_SIZE = 2048; /* Size of socket buffers in bytes (separate input and output buffers) */
constexpr auto PORT_RANGE = 0xFFFF; /* Maximum of ports */
constexpr uint32_t INET_SOCKET_ERROR = 0xFFFFFFFF; /* IFile calls on a TINet_Socket that return this value were erroneous */

/* Network socket status flags */
enum class NINet_Socket_Flags : uint8_t {
	CLOSED 		= 1,	/* Socket is closed */
	CONFIGURED	= 2,	/* Socket is configured */
	BOUND 		= 4,	/* Socket is bound to local interface */
	CONNECTED	= 8,	/* Socket is connected to remote host */

	ERROR		= 32,	/* Last operation caused an error */
	IN_FULL 	= 64,	/* Input buffer is full */
	OUT_FULL 	= 128,	/* Output buffer is full */
};

/* Network socket error codes */
enum class NINet_Socket_Errors : uint8_t {
	OK = 0,				/* No errors */
	CONFIGURE_FAIL,		/* Configuration of socket failed */
	BIND_FAIL,			/* Binding to local interface failed */
	CONNECT_FAIL,		/* Connecting to remote endpoint failed */
	SEND_BUFF_OVERFLOW,	/* No write was performed - not enough space in outbound buffer */
	RECV_BUFF_OVERFLOW,	/* Inbound buffer overflowed */
};

/* Network socket class */
class TINet_Socket : public IFile {
	// TODO

	private:
		TNetwork_Port loc_port, end_port;
		NInternet_Protocol ip;
		NTransport_Protocol tp;
		// TIP_Address<ip> loc_addr, end_addr; // Problem with templates? - maybe use struct* and reinterpret cast
		char in_buff[INET_SOCKET_BUFFER_SIZE];
		char out_buff[INET_SOCKET_BUFFER_SIZE];
		uint8_t flags;

	public:
		TINet_Socket();

		~TINet_Socket();

		uint8_t Configure(const NInternet_Protocol ip, const NTransport_Protocol tp);

		uint8_t Bind(TIP_Address_Container& local);

		uint8_t Connect(TIP_Address_Container& remote);

		uint32_t Send(const char* buff, const uint32_t num);

		uint32_t Send_To(const char* buff, const uint32_t num, TIP_Address_Container& addr);

		uint32_t Recv(char* buff, const uint32_t num);

		uint32_t Recv_From(char *buff, const uint32_t num, TIP_Address_Container& addr);



		// IFile implementation - all methods for future reference

		uint32_t Read(char* buffer, uint32_t num) override;

		uint32_t Write(const char* buffer, uint32_t num) override;

		bool Close() override;

		bool IOCtl(NIOCtl_Operation dir, void* ctlptr) override;

		bool Wait(uint32_t count) override;

		bool Try_Wait_All_Reserve(uint32_t count) override;

		bool Wait_All_Acquire(uint32_t count) override;

		uint32_t Notify(uint32_t count) override;

		void Wait_Enqueue_Current() override;

		bool Wait_Dequeue_Process(uint32_t pid) override;
};

class CINet {
	private:
		CUART& uart;
		TINet_Socket* bindings[PORT_RANGE] { nullptr }; /* Mapping of socket bindings - only simple array as just UDP is supported so we do not need to implement a multikey map yet - ports in network format are used as indices */
		

	public:
		CINet(CUART& uart);

		// TINet_Socket* Get_Socket(const TNetwork_Port port, const NTransport_Protocol protocol); // Returns pointer to socket (can be null if there is no socket with these identifiers), port is in host format
};

TNetwork_Port Host_To_Network(TNetwork_Port port);

TNetwork_Port Network_To_Host(TNetwork_Port port);

template <NInternet_Protocol IP>
TIP_Address<IP> Host_To_Network(TIP_Address<IP>);

template <NInternet_Protocol IP>
TIP_Address<IP> Network_To_Host(TIP_Address<IP>);