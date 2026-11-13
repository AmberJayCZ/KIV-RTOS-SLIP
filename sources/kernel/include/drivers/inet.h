#pragma once

#include <drivers/bridges/inet_defs.h>

// Provides address container initialized with IP version
TIP_Address_Container Get_Addr_Container(const NInternet_Protocol ip);

TNetwork_Port HToN_Port(const TNetwork_Port port);

TNetwork_Port NToH_Port(const TNetwork_Port port);

void HToN_Addr(TIP_Address_Container& addr, const NInternet_Protocol ip);

void NToH_Addr(TIP_Address_Container& addr, const NInternet_Protocol ip);

// Parses str according to ip from numbers and dots notation into NBO numeric format, returns prepared container
TIP_Address_Container AToN(const char* str, const NInternet_Protocol ip);

// Formats address from NBO numeric format into numbers and dots notation, returns pointer to internal field
char* NToA(const TIP_Address_Container& addr);

// Compares adresses. Returns true when ports and addresses match or when ports match, A is Any and B is not None. If addresses have different protocols, returns false.
bool Compare_Addresses(const TIP_Address_Container& a, const TIP_Address_Container& b);

uint32_t _get_header_length(const NInternet_Protocol ip);

// uint32_t _get_header_length(const TIP_Header_Pretext& p);

uint32_t _get_header_length(const NTransport_Protocol tp);

uint32_t _get_address_length(const NInternet_Protocol ip);

uint32_t _get_data_length(const char* h, const NTransport_Protocol tp);

// TODO: write
bool _parse_ip_header(const char* header_start, NInternet_Protocol& ip, NTransport_Protocol& tp, TIP_Address_Container& src, TIP_Address_Container& dst, uint32_t& ihl, uint32_t& total_length);

// TODO: write
bool _parse_tp_header(const char* header_start, const NTransport_Protocol tp, TIP_Address_Container& src, TIP_Address_Container& dst, uint32_t& thl, uint32_t& total_length);

// TODO: testing
uint32_t _create_headers(char* buff, const NInternet_Protocol ip, const NTransport_Protocol tp);

// TODO: testing
bool _fill_headers(char* buff, const NInternet_Protocol ip, const NTransport_Protocol tp, 
	const TIP_Address_Container& source, const TIP_Address_Container& destination, const uint32_t message_length);

// TODO: testing
bool _calculate_header_checksums(char* buff, const NInternet_Protocol ip, const NTransport_Protocol tp, 
	const TIP_Address_Container& source, const TIP_Address_Container& destination, const uint32_t message_length);

// TODO: testing
uint32_t _make_headers(char* buff, const NInternet_Protocol ip, const NTransport_Protocol tp, 
	const TIP_Address_Container& source, const TIP_Address_Container& destination, const uint32_t message_length);

// TODO: testing
bool _verify_header_checksums(const char* headers, const NInternet_Protocol ip, const uint32_t iph_length, const NTransport_Protocol tp);