#pragma once

#include <drivers/bridges/inet_defs.h>
#include <drivers/uart.h>
#include <fs/filesystem.h>

constexpr auto INET_SOCKET_BUFFER_SIZE = 2048; /* Size of socket buffers in bytes (separate input and output buffers) */
constexpr auto PORT_RANGE = 0xFFFF; /* Maximum of ports */

enum class NINet_Socket_Flags : uint16_t {
	CLOSED 		= 1,
	BOUND 		= 2,
	IN_FULL 	= 4,
	OUT_FULL 	= 8,
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
		uint16_t flags;
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