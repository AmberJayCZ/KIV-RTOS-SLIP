#pragma once

#include <hal/intdef.h>



/* Internet protocol identifier */
enum class NInternet_Protocol : uint8_t {
	V4 = 4,	/* Internet Protocol Version 4 (IPv4) */
};

/* Differentiated Services Code Point modes */
enum class NIP_DSCP : uint8_t {
	DF = 0,	/* Default Forwarding */
};

/* Explicit Congestion Notification flags */
enum class NIP_ECN : uint8_t {
	NotECT = 0b00,	/* Not ECN-capable transport */
	ECT1 = 0b01,	/* ECN-capable transport (1) */
	ECT0 = 0b10,	/* ECN-capable transport (0) */
	CE = 0b11,		/* Congestion experienced */
};

/* Special bitwise flags for IP packets */
enum class NIP_Flags : uint8_t {
	Reserved = 0b001,		/* Must not be set */
	DontFragment = 0b010,	/* Packet should not be fragmented */
	MoreFragments = 0b100,	/* If packet is fragmented then set on all fragments except the last one */
};



/* Transport protocol identifier */
enum class NTransport_Protocol : uint8_t {
	UDP = 17,	/* Universal Datagram Protocol */
};



/* Defines size (in bytes) of TIP_Address_Container typecasting struct */
constexpr int MAX_ADDR_LEN = 16;

/* Struct for typecasting IP addresses */
struct TIP_Address_Container {
	TNetwork_Port port; /* Target port (network format) */
	uint8_t raw_addr[MAX_ADDR_LEN]; /* Raw address in bytes (network format) - use reinterpret_cast to concrete TIP_Address */
};

/* Generic IP address type */
template <NInternet_Protocol IP>
struct TIP_Address_Type;

/* Generic IP address type alias */
template <NInternet_Protocol IP>
using TIP_Address = typename TIP_Address_Type<IP>::type;

/* IPv4 address type specialization */
template <>
struct TIP_Address_Type<NInternet_Protocol::V4> {
    using type = uint32_t; // IPv4 address stored as 32-bit integer
};

/* IPv4 address type alias */
using TIPv4_Address = TIP_Address<NInternet_Protocol::V4>;

/* Broadcast IPv4 address constant - use for broadcast or recieving from unknown address */
constexpr TIPv4_Address IPv4_Address_Any = 0xFFFFFFFF;

/* Invalid IPv4 address constant - recieving from this address will drop packet */
constexpr TIPv4_Address IPv4_Address_None = 0x00000000;



/* Network port type alias */
using TNetwork_Port = uint16_t;

/* Generic internet protocol header */
template <NInternet_Protocol IP>
struct TIP_Header;

#pragma pack(push, 1)

/* IPv4 header */
template <>
struct TIP_Header<NInternet_Protocol::V4> {
	uint8_t IPVersion : 4;				/* IP version (4b - NInternet_Protocol) */
	uint8_t IHL : 4;					/* IP header length in bytes (4b)  */
	uint8_t DSCP : 6;					/* DSCP mode (6b - NIP_DSCP) */
	uint8_t ECN : 2;					/* ECN flags (2b - NIP_ECN) */
	uint16_t Length;					/* Header + data length in bytes (16b) */
	uint16_t FragmentID;				/* Packet fragment group identification (16b) */
	uint8_t Flags : 3;					/* Special IP flags (3b - NIP_Flags) */
	uint16_t FragmentOffset : 13;		/* Position of fragment within packet measured in 8-byte units (16b) */
	uint8_t TTL;						/* Time-to-live counter (8b) */
	uint8_t DataProtocol;				/* Protocol for encapsulated data (8b - NTransport_Protocol) */
	uint16_t Checksum;					/* Header checksum (16b) */
	uint32_t SourceAddress;				/* Source IPv4 address (32b - TIPv4_Address) */
	uint32_t DestinationAddress;		/* Destination IPv4 address (32b - TIPv4_Address) */
	/* Options prozatim nepodporovane, mozna pozdeji */
};

#pragma pack(pop)

/* IPv4 header type alias */
using TIPv4_Header = TIP_Header<NInternet_Protocol::V4>;



/* Generic transport protocol header type */
template <NTransport_Protocol TP> 
struct TTP_Header;

/* UDP header */
template <>
struct TTP_Header<NTransport_Protocol::UDP> {
	uint16_t SourcePort;			/* Source network port (16b - TNetwork_Port) */
	uint16_t DestinationPort;		/* Destination network port (16b - TNetwork_Port) */
	uint16_t Length;				/* Header + data length in bytes (16b) */
	uint16_t Checksum;				/* UDP pseudo-header checksum (16b) */
};

/* UDP header type alias */
using TUDP_Header = TTP_Header<NTransport_Protocol::UDP>;