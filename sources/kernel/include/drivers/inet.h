#pragma once

#include <drivers/bridges/inet_defs.h>

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