# Notes

## New files

- `sources`
  - `kernel`
    - `include`
      - `drivers`
        - `bridges`
          - `inet_defs.h` - Definitions of fundamental structs and enums
        - `inet.h` - Helper conversion functions
      - `fs`
        - `drivers`
          - `inet_fs.h` - Defines generic network socket and driver interface + socket provider class where drivers register their sockets
          - `slip_fs.h` - Provides SLIP socket and driver implementation
    - `src`

## Modified files

- `sources`
  - `kernel`
    - `include`
      - `fs`
        - `filesystem.h` - Added `NFile_Type_Major::Network_Socket` file type

## Filesystem path parsing

SLIP driver will support socket creation via a specified filesystem path.
Root node is `DEV:slip`. Opening file here creates an uninitialized socket. Additional parameters can be specified using 'directories' (separated by forward slash `/`).
The proposed structure is:

- `DEV:slip` - root node
  - `<IP>-<TP>` - numeric IP version (`4` for IPv4) and text TP identifier (`UDP`)
    - `<local IP address>-<local port>` - IP address in text notation and port in numeric notation (host format)
      - `<remote IP address>-<remote port>`

## Concept (outdated)

INet driver - manages filesystem sockets
UART SLIP driver - handles instantiation of sockets that communicate via SLIP over UART
- Socket - has only input buffer, which SLIP driver writes into
- Driver - has output formatting buffer (socket send() -> output buffer -> SILP format and send) and input reassemble buffer (SLIP recieve -> unformat -> parse headers -> socket recieve buffer)
  - Also has a mutex for writing to output buffer by sockets
  - Output buffer is output to UART after each Send() call
  - Input buffer moves message to target socket buffer after reassemble
- Socket - AUX output buffer for transforming message to SLIP format

Control flow:
Driver continuously polls UART and output queue
- Reads UART data and extracts SLIP tags -> processed message is stored into buffer (at least header and partial data) -> bytes in buffer are interpreted and sent to target socket (even if only partial data then we still know how much more data is waiting)
- Output queue contains formatted messages -> dump to UART for sending
Sockets send on demand when send method is called - use aux buffer to format message and write formatted to driver buffer

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

template struct IP_Header<Internet_Protocol>;

template struct TP_Header<Transport_Protocol>;

namespace SLIP {
	// Create socket with specified IP and TP versions
	In_Socket<Internet_Protocol, Transport_Protocol> socket(Internet_Protocol ip, Transport_Protocol tp);

	// Binding local address
	int In_Socket<Internet_Protocol, Transport_Protocol>::bind(addr_t<Internet_Protocol>& addr);

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
