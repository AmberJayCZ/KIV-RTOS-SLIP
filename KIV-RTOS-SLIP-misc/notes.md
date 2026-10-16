# Notes

## Concept

```cpp
// NOTE: This is a different approach from standard UNIX-like sockets and file descriptors - needs to be discussed,
// will be perhaps reworked to function more like BSD sockets and use the VFS API in stdlib/stdfile
// 
// Also it is a first draft, so there is that...

enum class Internet_Protocol {
	V4 = 4,
};

enum class Transport_Protocol {
	UDP = 17,
};

typedef uint32_t IPv4_Addr;

typedef uint16_t In_Port;

template struct IP_Header<Internet_Protocol>; // Is this even possible???

template struct TP_Header<Internet_Protocol, Transport_Protocol>;

struct IP4_Header;

struct UDP4_Header;

namespace SLIP {
	// Create socket with specified IP and TP versions
	In_Socket<Internet_Protocol, Transport_Protocol> socket(Internet_Protocol ip, Transport_Protocol tp);

	// Binding local address
	int In_Socket<Internet_Protocol, Transport_Protocol>::bind(addr_t<Internet_Protocol>& addr);
	// NOTE: It would be preferable if addr_t was implicitely tied to IP version - so eventually binding an IPv6 addr to IPv4 socket is caught even in IDE

	// Binding endpoint address
	int In_Socket<Internet_Protocol, Transport_Protocol>::connect(addr_t<Internet_Protocol>& addr);

	// Sending to connected endpoint - TCP + UDP
	size_t In_Socket<Internet_Protocol, Transport_Protocol>::send(char* data, size_t size);

	// Sending to unbound endpoint - only UDP
	size_t In_Socket<Internet_Protocol, Transport_Protocol>::send(char* data, size_t size, addr_t<Internet_Protocol>& addr)

	// Recieving from connected endpoint - TCP + UDP
	size_t In_Socket<Internet_Protocol, Transport_Protocol>::recv(char* buffer, size_t size);

	// Recieving from unbound endpoint - only UDP
	size_t In_Socket<Internet_Protocol, Transport_Protocol>::recv(char* buffer, size_t size, addr_t<Internet_Protocol>& addr);

	// Can also be called from FS like this (if possible)
	// uint32_t socket = open(":sock/IP_Version/TP_Version/Binding_Port@Binding_Addr->Connected_Port@Connected_Addr");

	// Maps specific sockets to VFS identifiers
	// NOTE: VFS functions like open(), read(), write() will be bound to functions in here
	map<uint32_t, In_Socket> m;

	// Retrieves header currently set for socket (probably useful only if you want set unusual parameters directly for send())
	struct IP_Header<Internet_Protocol>& In_Socket<Internet_Protocol, Transport_Protocol>::Get_IP_Header();

	// Sets header for socket - similar to above (note that header fields may be overwritten if some functions are called afterwards before sending)
	bool In_Socket<Internet_Protocol, Transport_Protocol>::Set_IP_Header(struct IP_Header<Internet_Protocol>& header);

	struct TP_Header<Transport_Protocol>& In_Socket<Internet_Protocol, Transport_Protocol>::Get_TP_Header();

	bool In_Socket<Internet_Protocol, Transport_Protocol>::Set_TP_Header(struct TP_Header<Transport_Protocol>& header);
}
```

## Resources

[SLIP](https://en.wikipedia.org/wiki/Serial_Line_Internet_Protocol)

[IPv4 header](https://en.wikipedia.org/wiki/IPv4#Header)

[UDP header](https://en.wikipedia.org/wiki/User_Datagram_Protocol#UDP_datagram_structure)

[IP packet options](https://en.wikipedia.org/wiki/Internet_Protocol_Options)
