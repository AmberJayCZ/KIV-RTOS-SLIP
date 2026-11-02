#pragma once

#include <drivers/bridges/inet_defs.h>
#include <stdstring.h>

/* TODO */
TNetwork_Port Host_To_Network(const TNetwork_Port port);

/* TODO */
TNetwork_Port Network_To_Host(const TNetwork_Port port);

/* TODO */
template <NInternet_Protocol IP>
TIP_Address<IP> Host_To_Network(const TIP_Address<IP>);

/* TODO */
template <NInternet_Protocol IP>
TIP_Address<IP> Network_To_Host(const TIP_Address<IP>);

/* TODO */
template <NInternet_Protocol IP>
TIP_Address<IP> Address_To_Number(const char* str); // Would return IPv4_Address_None on conversion failure - maybe use pointers for conversion so that a potentially valid output value is not used to indicate an error?

/* TODO */
template <NInternet_Protocol IP>
char* Number_To_Address(const TIP_Address<IP>);


/* Provides zeroed TIP_Address_Container struct */
TIP_Address_Container Get_Addr_Container() {
	TIP_Address_Container c;
	c.port = 0;
	memset(c.raw_addr, 0, MAX_ADDR_LEN);
	return c;
}

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

inline uint32_t Get_IP_Address_Length(const NInternet_Protocol protocol) {
	switch (protocol) {
		case NInternet_Protocol::V4:
			return sizeof(TIPv4_Address);
		
		default:
			return 0;
	}
}

inline uint32_t Get_IHL(const NInternet_Protocol protocol) {
	switch (protocol) {
		case NInternet_Protocol::V4:
			return sizeof(TIPv4_Header);
		
		default:
			return 0;
	}
}

inline uint32_t Get_THL(const NTransport_Protocol protocol) {
	switch (protocol) {
		case NTransport_Protocol::UDP:
			return sizeof(TUDP_Header);
		
		default:
			return 0;
	}
}

/* TODO - testing */
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

/* TODO - testing */
bool Fill_Headers(char* buff, const NInternet_Protocol ip, const NTransport_Protocol tp, 
	const TIP_Address_Container& source, const TIP_Address_Container& destination, const uint32_t message_length) {
	uint32_t offset = 0;
	
	switch (ip) {
		case NInternet_Protocol::V4:
			offset = sizeof(TIPv4_Header);
			auto header = reinterpret_cast<TIPv4_Header*>(buff);
			header->DataProtocol = static_cast<uint8_t>(NInternet_Protocol::V4);
			header->IHL = 5; /* Fixed to 5B as options are not supported ATP */
			// header->DSCP = static_cast<uint8_t>(NIP_DSCP::DF);	/* Commented because we use 0 values either way ATP */
			// header->ECN = static_cast<uint8_t>(NIP_ECN::NotECT);
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

/* TODO - testing */
uint16_t Calculate_1s_Complement_Sum(const uint16_t a, const uint16_t b) {
	uint16_t res = a + b;

	if (res < a) /* Bit overflow => add LSB */
		res++;

	return res;
}

/* Extracts IP version from IP header beginning - TODO rethink maybe, if invalid value arrives then what */
inline NInternet_Protocol Extrapolate_IP(const char* data) {
	return static_cast<NInternet_Protocol>(reinterpret_cast<const TIP_Header_Pretext*>(data)->IPVersion);
}

/* TODO - same as above */
NTransport_Protocol Extrapolate_TP(const char* data, const NInternet_Protocol ip) {
	switch (ip) {
		case NInternet_Protocol::V4:
			auto header = reinterpret_cast<const TIPv4_Header*>(data);
			return static_cast<NTransport_Protocol>(header->DataProtocol);
		
		default:
			return NTransport_Protocol::UDP;
	}
}

/* Parses data and returns IP header version with addresses */
bool Extract_IP_Addresses(const char* data, TIP_Address_Container& source, TIP_Address_Container& destination, const NInternet_Protocol ip) {
	switch (ip) {
		case NInternet_Protocol::V4:
			auto header = reinterpret_cast<const TIPv4_Header*>(data);
			auto src = reinterpret_cast<TIPv4_Address>(source.raw_addr);
			src = header->SourceAddress;
			auto dst = reinterpret_cast<TIPv4_Address>(source.raw_addr);
			dst = header->DestinationAddress;
			return true;

		default:
			return false;
	}
}

/* TODO - testing */
bool Calculate_Header_Checksums(char* buff, const NInternet_Protocol ip, const NTransport_Protocol tp, 
	const TIP_Address_Container& source, const TIP_Address_Container& destination, const uint32_t message_length) {
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
			auto tp_header = reinterpret_cast<TUDP_Header*>(&buff[offset]);
			uint16_t sum = 0;
			uint16_t tmp[1] = { 0 }; /* 8b zeroes, 8b protocol, 16b datagram length */
			// reinterpret_cast<uint8_t*>(tmp)[0] = 0;
			reinterpret_cast<uint8_t*>(tmp)[1] = static_cast<uint8_t>(tp);
			sum = Calculate_1s_Complement_Sum(sum, tmp[0]);
			sum = Calculate_1s_Complement_Sum(sum, static_cast<uint16_t>(Get_THL(tp) + message_length));
			const auto addr_len_16 = Get_IP_Address_Length(ip) / 2;

			auto a = reinterpret_cast<const uint16_t*>(source.raw_addr);
			auto b = reinterpret_cast<const uint16_t*>(destination.raw_addr);

			for (auto i = 0; i < addr_len_16; i++) {
				sum = Calculate_1s_Complement_Sum(sum, a[i]);
				sum = Calculate_1s_Complement_Sum(sum, b[i]);
			}

			tp_header->Checksum = ~(sum & 0xFFFF);

			// If calculated checksum is 0, it may be interpreted as no checksum, so set all 1s instead
			if (tp_header->Checksum == 0)
				tp_header->Checksum = 0xFFFF;

			break;
		
		default:
			return false;
	}

	return true;
}

/* TODO - testing */
uint32_t Make_Headers(char* buff, const NInternet_Protocol ip, const NTransport_Protocol tp, 
	const TIP_Address_Container& source, const TIP_Address_Container& destination, const uint32_t message_length) {
	uint32_t written = Create_Headers(buff, ip, tp);
	
	if (!Fill_Headers(buff, ip, tp, source, destination, message_length))
		return 0;

	if (!Calculate_Header_Checksums(buff, ip, tp, source, destination, message_length))
		return 0;

	return written;
}

/* TODO - testing */
bool Verify_Header_Checksums(const char* data) {
	uint32_t offset = 0;
	TIP_Address_Container src = Get_Addr_Container();
	TIP_Address_Container dst = Get_Addr_Container();

	const NInternet_Protocol ip = Extrapolate_IP(data);
	const NTransport_Protocol tp = Extrapolate_TP(data, ip);
	Extract_IP_Addresses(data, src, dst, ip);
	
	switch (ip) {
		case NInternet_Protocol::V4:
			auto size = Get_IHL(ip);
			offset = size;
			auto header = reinterpret_cast<const TIPv4_Header*>(data);
			uint16_t sum = 0;
			auto header_arr = reinterpret_cast<const uint16_t*>(header);

			for (auto i = 0; i < size / 2; i++) { /* IP checksum is calculated using 16b words */
				sum = Calculate_1s_Complement_Sum(sum, header_arr[i]);
			}

			if (sum != 0)
				return false;

			break;
		
		default:
			return false;
	}

	switch (tp) {
		case NTransport_Protocol::UDP:
			auto header = reinterpret_cast<const TUDP_Header*>(&data[offset]);
			uint16_t sum = 0;
			uint16_t tmp[1] = { 0 }; /* 8b zeroes, 8b protocol, 16b datagram length */
			// reinterpret_cast<uint8_t*>(tmp)[0] = 0;
			reinterpret_cast<uint8_t*>(tmp)[1] = static_cast<uint8_t>(tp);
			sum = Calculate_1s_Complement_Sum(sum, tmp[0]);
			sum = Calculate_1s_Complement_Sum(sum, header->Length);
			const auto addr_len_16 = Get_IP_Address_Length(ip) / 2;

			auto a = reinterpret_cast<const uint16_t*>(src.raw_addr);
			auto b = reinterpret_cast<const uint16_t*>(dst.raw_addr);

			for (auto i = 0; i < addr_len_16; i++) {
				sum = Calculate_1s_Complement_Sum(sum, a[i]);
				sum = Calculate_1s_Complement_Sum(sum, b[i]);
			}

			sum = Calculate_1s_Complement_Sum(sum, header->Checksum);

			// Calculated sum should be 0
			return sum == 0;
		
		default:
			return false;
	}

	return true;
}