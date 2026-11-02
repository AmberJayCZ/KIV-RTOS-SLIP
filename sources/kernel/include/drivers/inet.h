#pragma once

#include <drivers/bridges/inet_defs.h>
#include <stdstring.h>

TNetwork_Port Host_To_Network(const TNetwork_Port port);

TNetwork_Port Network_To_Host(const TNetwork_Port port);

template <NInternet_Protocol IP>
TIP_Address<IP> Host_To_Network(const TIP_Address<IP>);

template <NInternet_Protocol IP>
TIP_Address<IP> Network_To_Host(const TIP_Address<IP>);

template <NInternet_Protocol IP>
TIP_Address<IP> Address_To_Number(const char* str); // Would return IPv4_Address_None on conversion failure - maybe use pointers for conversion so that a potentially valid output value is not used to indicate an error?

template <NInternet_Protocol IP>
char* Number_To_Address(const TIP_Address<IP>);

bool Compare_Addresses(const TIP_Address_Container& a, const TIP_Address_Container& b, const NInternet_Protocol protocol) {
	switch (protocol) {
		case NInternet_Protocol::V4:
			auto ar = reinterpret_cast<const TIPv4_Address>(a.raw_addr);
			auto br = reinterpret_cast<const TIPv4_Address>(b.raw_addr);
			return a.port == b.port && ar == br;
	
		default:
			return false;
	}
}

uint32_t Get_IHL(const NInternet_Protocol protocol) {
	switch (protocol) {
		case NInternet_Protocol::V4:
			return sizeof(TIPv4_Header);
		
		default:
			return 0;
	}
}

uint32_t Get_THL(const NTransport_Protocol protocol) {
	switch (protocol) {
		case NTransport_Protocol::UDP:
			return sizeof(TUDP_Header);
		
		default:
			return 0;
	}
}

uint32_t Create_Headers(char* buff, const NInternet_Protocol ip, const NTransport_Protocol tp) {
	uint32_t written = 0;
	uint32_t size = 0;

	switch (ip) {
		case NInternet_Protocol::V4:
			size = sizeof(TIPv4_Header);
			memset(buff, 0, size);
			auto header = reinterpret_cast<TIPv4_Header*>(buff);
			written += size;
			break;
		
		default:
			return 0;
	}

	switch (tp) {
		case NTransport_Protocol::UDP:
			size = sizeof(TUDP_Header);
			memset(&buff[written], 0, size);
			auto header = reinterpret_cast<TUDP_Header*>(&buff[written]);
			written += size;
			break;
		
		default:
			return 0;
	}

	return written;
}

bool Fill_Headers(char* buff, const NInternet_Protocol ip, const NTransport_Protocol tp, 
	const TIP_Address_Container& source, const TIP_Address_Container& destination, const uint32_t message_length) {
	uint32_t offset = 0;
	
	switch (ip) {
		case NInternet_Protocol::V4:
			offset = sizeof(TIPv4_Header);
			auto header = reinterpret_cast<TIPv4_Header*>(buff);
			header->DataProtocol = static_cast<uint8_t>(NInternet_Protocol::V4);
			header->IHL = 5; /* Fixed to 5B as options are not supported ATP */
			header->DSCP = static_cast<uint8_t>(NIP_DSCP::DF);
			header->Length = static_cast<uint16_t>(Get_IHL(ip) + Get_THL(tp) + message_length);
			header->FragmentID = 0; /* No fragmentation support */
			header->Flags = static_cast<uint8_t>(NIP_Flags::DontFragment); /* Fragmentation explicitly disabled */
			header->FragmentOffset = 0;
			header->TTL = Default_TTL;
			header->DataProtocol = static_cast<uint8_t>(tp);
			header->SourceAddress = reinterpret_cast<TIPv4_Address>(source.raw_addr);
			header->DestinationAddress = reinterpret_cast<TIPv4_Address>(destination.raw_addr);
			break;
		
		default:
			return false;
	}

	switch (tp) {
		case NTransport_Protocol::UDP:
			auto header = reinterpret_cast<TUDP_Header*>(&buff[offset]);
			header->SourcePort = source.port;
			header->DestinationPort = destination.port;
			header->Length = static_cast<uint16_t>(Get_THL(tp) + message_length);
			break;
		
		default:
			return false;
	}

	return true;
}

uint16_t Calculate_1s_Complement_Sum(uint16_t a, uint16_t b) {
	uint16_t res = a + b;

	if (res < a) /* Bit overflow => add LSB */
		res++;

	return res;
}

bool Calculate_Header_Checksums(char* buff, const NInternet_Protocol ip, const NTransport_Protocol tp, 
	const TIP_Address_Container& source, const TIP_Address_Container& destination) {
	uint32_t offset = 0;
	
	switch (ip) {
		case NInternet_Protocol::V4:
			auto size = Get_IHL(ip);
			offset = size;
			auto header = reinterpret_cast<TIPv4_Header*>(buff);
			uint16_t sum = 0;
			auto header_arr = reinterpret_cast<uint16_t*>(header);

			for (auto i = 0; i < size / 2; i++) { /* IP checksum is calculated using 16b words */
				sum = Calculate_1s_Complement_Sum(sum, header_arr[i]);
			}

			header->Checksum = ~(sum & 0xFFFF);
			break;
		
		default:
			return false;
	}

	switch (tp) {
		case NTransport_Protocol::UDP:
			auto size = Get_THL(tp);
			auto header = reinterpret_cast<TUDP_Header*>(&buff[offset]);
			uint16_t sum = 0;
			auto header_arr = reinterpret_cast<uint16_t*>(header);

			for (auto i = 0; i < size / 2; i++) { /* Checksum is calculated using 16b words */
				sum = Calculate_1s_Complement_Sum(sum, header_arr[i]);
			}

			header->Checksum = ~(sum & 0xFFFF);
			break;
		
		default:
			return false;
	}

	return true;
}

uint32_t Make_Headers(char* buff, const NInternet_Protocol ip, const NTransport_Protocol tp, 
	const TIP_Address_Container& source, const TIP_Address_Container& destination, const uint32_t message_length) {
	uint32_t written = Create_Headers(buff, ip, tp);
	
	if (!Fill_Headers(buff, ip, tp, source, destination, message_length))
		return 0;

	if (!Calculate_Header_Checksums(buff, ip, tp, source, destination))
		return 0;

	return written;
}