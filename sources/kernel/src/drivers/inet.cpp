#include <drivers/inet.h>
#include <fs/drivers/inet_fs.h>
#include <stdstring.h>

constexpr auto _TB_SIZE = 16;
char _addr_tb[_TB_SIZE];

// ======================= Internal functions 1 =========================

uint16_t _rbo16(const uint16_t val) {
	return (((val << 8) & 0xFF00) | ((val >> 8) & 0xFF00));
}

uint32_t _rbo32(const uint32_t val) {
	return (
		((val << 24) & 0xFF000000) | 
		((val <<  8) & 0x00FF0000) |
		((val >>  8) & 0x0000FF00) |
		((val >> 24) & 0x000000FF)
	);
}

void _rbo_buffer(uint8_t* data, const uint32_t size) {
	if (!data || size < 2) return;
    
    uint32_t left = 0;
    uint32_t right = size - 1;
    
    while (left < right) {
        uint8_t temp = data[left];
        data[left] = data[right];
        data[right] = temp;
        
        left++;
        right--;
    }
}

// TODO: testing
uint16_t _calculate_1s_complement_sum(const uint16_t a, const uint16_t b) {
	uint16_t res = a + b;

	if (res < a) /* Bit overflow => add LSB */
		res++;

	return res;
}

// ======================= Interface functions =====================

TIP_Address_Container Get_Addr_Container(const NInternet_Protocol ip) {
	TIP_Address_Container c { 0, ip, false };
	memset(c.raw_addr, 0, MAX_ADDR_LEN);
	return c;
}

bool Compare_Addresses(const TIP_Address_Container& a, const TIP_Address_Container& b) {
	if (a._ip != b._ip)
		return false;
	
	switch (a._ip) {
		case NInternet_Protocol::V4: {
			auto ra = reinterpret_cast<const TIPv4_Address*>(&a.raw_addr);
			auto rb = reinterpret_cast<const TIPv4_Address*>(&b.raw_addr);
			return a.port == b.port && (*ra == *rb || (*ra == IPv4_Address_Any && *rb != IPv4_Address_None));
		}
	
		default:
			return false;
	}
}

TNetwork_Port HToN_Port(const TNetwork_Port port) {
	return _rbo16(port);
}

TNetwork_Port NToH_Port(const TNetwork_Port port) {
	return _rbo16(port);
}

void HToN_Addr(TIP_Address_Container& addr, const NInternet_Protocol ip) {
	if (addr._nbo) return;
	
	if (ip == NInternet_Protocol::V4) {
		TIPv4_Address& a = reinterpret_cast<uint32_t&>(addr.raw_addr);
		a = _rbo32(a);
	}
}

void NToH_Addr(TIP_Address_Container& addr, const NInternet_Protocol ip) {
	if (!addr._nbo) return;
	
	if (ip == NInternet_Protocol::V4) {
		TIPv4_Address& a = reinterpret_cast<uint32_t&>(addr.raw_addr);
		a = _rbo32(a);
	}
}

TIP_Address_Container AToN(const char* str, const NInternet_Protocol ip) {
	TIP_Address_Container addr = Get_Addr_Container(ip);

	if (!str)
		return addr;

	if (ip == NInternet_Protocol::V4) {
		unsigned char octets[4];
		int octet_index = 0;
		int current_val = -1;
		int digit_count = 0;
		bool valid = true;

		while (*str != '\0' && valid) {
			char c = *str;

			if (c >= '0' && c <= '9') {
				// Disallow leading zeros for individual octets (e.g., "192.168.01.1" is invalid)
				if (digit_count == 1 && current_val == 0) {
					valid = false;
				}

				if (current_val == -1) {
					current_val = 0;
				}

				current_val = current_val * 10 + (c - '0');
				
				// Octet value must be between 0 and 255
				if (current_val > 255) {
					valid = false;
				}

				digit_count++;
				// Maximum 3 digits per octet
				if (digit_count > 3) {
					valid = false;
				}
			}

			else if (c == '.') {
				// Must have at least one digit before a dot, and cannot exceed 3 dots
				if (digit_count == 0 || octet_index >= 3) {
					valid = false;
				}

				octets[octet_index++] = static_cast<unsigned char>(current_val);
				current_val = -1;
				digit_count = 0;
			}

			else {
				// Any character other than digits or dots is invalid
				valid = false;
			}

			str++;
		}

		// Must have exactly 4 octets (3 dots) and the final octet must terminate with digits
		if (!valid || octet_index != 3 || digit_count == 0) {
			// addr.raw_addr = reinterpret_cast<uint8_t*>(a);
			*reinterpret_cast<uint32_t*>(addr.raw_addr) = IPv4_Address_None;
			return addr;
		}

		octets[octet_index] = static_cast<unsigned char>(current_val);
		auto ap = reinterpret_cast<unsigned char*>(addr.raw_addr);

		for (auto i = 0; i <= 3; i++) {
			ap[i] = octets[i];
		}

		addr._nbo = true;
		return addr;
	}

	return addr;
}

char* NToA(const TIP_Address_Container& addr) {
	memset(_addr_tb, 0, _TB_SIZE);

	switch (addr._ip) {
		case NInternet_Protocol::V4: {
			auto idx = 0;

			for (auto i = 0; i < 3; i++) {
				itoa(addr.raw_addr[i], &_addr_tb[idx], 10);

				while (_addr_tb[idx] != '\0')
					idx++;

				_addr_tb[idx] = '.';
				idx++;
			}

			itoa(addr.raw_addr[3], &_addr_tb[idx], 10);

			return _addr_tb;
		}
		
		default:
			return _addr_tb;
	}
}

// ======================= Internal functions 2 ================================

uint32_t _get_header_length(const NInternet_Protocol ip) {
	if (ip == NInternet_Protocol::V4) return sizeof(TIPv4_Header);
	return 0;
}

uint32_t _get_header_length(const NTransport_Protocol tp) {
	if (tp == NTransport_Protocol::UDP) return sizeof(TUDP_Header);
	return 0;
}

uint32_t _get_address_length(const NInternet_Protocol ip) {
	if (ip == NInternet_Protocol::V4) return 4;
	return 0;
}

// Gets payload length from TP header
uint32_t _get_data_length(const char* h, const NTransport_Protocol tp) {
	if (tp == NTransport_Protocol::UDP) return NToH_Port(reinterpret_cast<const TUDP_Header*>(h)->Length) - sizeof(TUDP_Header);
	return 0;
}

// Selects address parsing function based on IP version
bool _parse_addr_str(const char* str, const NInternet_Protocol ip, TIP_Address_Container& addr) {
	switch (ip) {
		case NInternet_Protocol::V4: {
			addr = AToN(str, NInternet_Protocol::V4);
			return true;
		}
	
		default:
			return false;
	}
}

// Parses address from bytes in NBO
bool _parse_addr_bytes(const void* bytes, const NInternet_Protocol ip, TIP_Address_Container& addr) {
	switch (ip) {
		case NInternet_Protocol::V4: {
			auto r = reinterpret_cast<const uint8_t*>(bytes);

			for (auto i = 0; i < 4; i++) {
				addr.raw_addr[i] = r[i];
			}

			addr._ip = ip;
			addr._nbo = true;
			return true;
		}
		
		default:
			return false;
	}
}

// TODO: testing
// Zeroes buffer bytes for headers
uint32_t _create_headers(char* buff, const NInternet_Protocol ip, const NTransport_Protocol tp) {
	uint32_t written = 0;
	uint32_t size = 0;

	switch (ip) {
		case NInternet_Protocol::V4: {
			size = sizeof(TIPv4_Header);
			memset(buff, 0, size);
			written += size;
			break;
		}
		
		default:
			return 0;
	}

	switch (tp) {
		case NTransport_Protocol::UDP: {
			size = sizeof(TUDP_Header);
			memset(&buff[written], 0, size);
			written += size;
			break;
		}
		
		default:
			return 0;
	}

	return written;
}

// TODO: testing
// Fills headers
bool _fill_headers(char* buff, const NInternet_Protocol ip, const NTransport_Protocol tp, 
	const TIP_Address_Container& source, const TIP_Address_Container& destination, const uint32_t message_length) {
	uint32_t offset = 0;
	
	switch (ip) {
		case NInternet_Protocol::V4: {
			offset = sizeof(TIPv4_Header);
			auto header = reinterpret_cast<TIPv4_Header*>(buff);
			header->IPVersion = static_cast<uint8_t>(NInternet_Protocol::V4);
			header->IHL = 5; /* Fixed to 5*4 B as options are not supported ATP */
			header->DSCP = static_cast<uint8_t>(NIP_DSCP::DF);
			header->ECN = static_cast<uint8_t>(NIP_ECN::NotECT);
			header->Length = HToN_Port(static_cast<uint16_t>(header->IHL * 4 + _get_header_length(tp) + message_length));
			header->FragmentID = 0; /* No fragmentation support */
			header->Flags = 0;
			header->FragmentOffset = 0;
			header->TTL = Default_TTL;
			header->DataProtocol = static_cast<uint8_t>(tp);
			header->Checksum = 0; // Checksums are zeroed prior to calculation
			auto src = reinterpret_cast<const TIPv4_Address*>(&source.raw_addr);
			auto dst = reinterpret_cast<const TIPv4_Address*>(&destination.raw_addr);
			header->SourceAddress = *src;
			header->DestinationAddress = *dst;
			break;
		}
		
		default:
			return false;
	}

	switch (tp) {
		case NTransport_Protocol::UDP: {
			auto header = reinterpret_cast<TUDP_Header*>(&buff[offset]);
			header->SourcePort = source.port;
			header->DestinationPort = destination.port;
			header->Length = HToN_Port(static_cast<uint16_t>(_get_header_length(tp) + message_length));
			header->Checksum = 0; // Checksums are zeroed prior to calculation
			return true;
		}
		
		default:
			return false;
	}
}

// TODO: testing
// Calculates header checksums
bool _calculate_header_checksums(char* buff, const NInternet_Protocol ip, const NTransport_Protocol tp, 
	const TIP_Address_Container& source, const TIP_Address_Container& destination, const uint32_t message_length) {
	uint32_t offset = 0; // Offset of TP header from buff
	
	switch (ip) {
		case NInternet_Protocol::V4: {
			auto header = reinterpret_cast<TIPv4_Header*>(buff);
			auto size = header->IHL * 4;
			offset = size;
			uint16_t sum = 0;
			auto header_arr = reinterpret_cast<uint16_t*>(header);

			for (auto i = 0; i < size / 2; i++) { /* IP checksum is calculated using 16b words */
				sum = _calculate_1s_complement_sum(sum, header_arr[i]);
			}

			header->Checksum = ~(sum & 0xFFFF);
			break;
		}
		
		default:
			return false;
	}

	switch (tp) {
		case NTransport_Protocol::UDP: {
			auto tp_header = reinterpret_cast<TUDP_Header*>(&buff[offset]);
			uint16_t sum = 0;
			uint8_t tmp[2] = { 0, 0 }; /* 8b zeroes, 8b protocol, 16b datagram length */
			tmp[1] = static_cast<uint8_t>(tp);
			sum = _calculate_1s_complement_sum(sum, *reinterpret_cast<uint16_t*>(tmp));
			sum = _calculate_1s_complement_sum(sum, HToN_Port(static_cast<uint16_t>(_get_header_length(tp) + message_length)));
			const auto addr_len_16 = _get_address_length(ip) / 2;

			auto a = reinterpret_cast<const uint16_t*>(source.raw_addr);
			auto b = reinterpret_cast<const uint16_t*>(destination.raw_addr);

			for (auto i = 0; i < addr_len_16; i++) {
				sum = _calculate_1s_complement_sum(sum, a[i]);
				sum = _calculate_1s_complement_sum(sum, b[i]);
			}

			tp_header->Checksum = ~(sum & 0xFFFF);

			// If calculated checksum is 0, it may be interpreted as no checksum, so set all 1s instead
			if (tp_header->Checksum == 0)
				tp_header->Checksum = 0xFFFF;

			return true;
		}
		
		default:
			return false;
	}
}

// TODO: testing
// Creates headers, fills them and calculates checksums
uint32_t _make_headers(char* buff, const NInternet_Protocol ip, const NTransport_Protocol tp, 
	const TIP_Address_Container& source, const TIP_Address_Container& destination, const uint32_t message_length) {
	uint32_t written = _create_headers(buff, ip, tp);
	
	if (!_fill_headers(buff, ip, tp, source, destination, message_length))
		return 0;

	if (!_calculate_header_checksums(buff, ip, tp, source, destination, message_length))
		return 0;

	return written;
}

// TODO: testing
// Verifies checksums
bool _verify_header_checksums(const char* headers, const NInternet_Protocol ip, const uint32_t iph_length, const NTransport_Protocol tp) {
	const void* src_addr;
	const void* dst_addr;
	
	switch (ip) {
		case NInternet_Protocol::V4: {
			auto header = reinterpret_cast<const TIPv4_Header*>(headers);
			uint16_t sum = 0;
			src_addr = &header->SourceAddress;
			dst_addr = &header->DestinationAddress;
			auto header_arr = reinterpret_cast<const uint16_t*>(header);

			for (auto i = 0; i < iph_length / 2; i++) { // IP checksum is calculated using 16b words 
				sum = _calculate_1s_complement_sum(sum, header_arr[i]);
			}

			if (sum != 0xFFFF) // Sum should be all 1s
				return false;

			break; // IP header sum is ok, continue to TP header
		}
		
		default:
			return false;
	}

	switch (tp) {
		case NTransport_Protocol::UDP: {
			auto header = reinterpret_cast<const TUDP_Header*>(&headers[iph_length]);
			uint16_t sum = 0;
			uint8_t tmp[2] = { 0, static_cast<uint8_t>(tp) }; /* 8b zeroes, 8b protocol, 16b datagram length */
			sum = _calculate_1s_complement_sum(sum, *reinterpret_cast<uint16_t*>(tmp));
			sum = _calculate_1s_complement_sum(sum, header->Length);

			auto a = reinterpret_cast<const uint16_t*>(src_addr);
			auto b = reinterpret_cast<const uint16_t*>(dst_addr);

			for (auto i = 0; i < _get_address_length(ip) / 2; i++) {
				sum = _calculate_1s_complement_sum(sum, a[i]);
				sum = _calculate_1s_complement_sum(sum, b[i]);
			}

			sum = _calculate_1s_complement_sum(sum, header->Checksum);

			// Calculated sum should be all 1s
			return sum == 0xFFFF;
		}
		
		default:
			return false;
	}
}

// TODO: write
// Parses IP header
bool _parse_ip_header(const char* header_start, NInternet_Protocol& ip, NTransport_Protocol& tp, TIP_Address_Container& src, TIP_Address_Container& dst, uint32_t& ihl, uint32_t& total_length) {
	ip = static_cast<NInternet_Protocol>(reinterpret_cast<const TIP_Header_Pretext*>(header_start)->IPVersion);

	switch (ip) {
		case NInternet_Protocol::V4: {
			auto h = reinterpret_cast<const TIPv4_Header*>(header_start);
			tp = static_cast<NTransport_Protocol>(h->DataProtocol);
			_parse_addr_bytes(&h->SourceAddress, ip, src);
			_parse_addr_bytes(&h->DestinationAddress, ip, dst);
			ihl = h->IHL * 4;
			total_length = NToH_Port(h->Length);
			return true;
		}

		default:
			return false;
	}
}

// TODO: write
// Parses TP header
bool _parse_tp_header(const char* header_start, const NTransport_Protocol tp, TIP_Address_Container& src, TIP_Address_Container& dst, uint32_t& thl, uint32_t& total_length) {
	switch (tp) {
		case NTransport_Protocol::UDP: {
			auto h = reinterpret_cast<const TUDP_Header*>(header_start);
			src.port = h->SourcePort;
			dst.port = h->DestinationPort;
			thl = sizeof(TUDP_Header);
			total_length = NToH_Port(h->Length);
		}

		default:
			return false;
	}
}

// ======================= Internet Socket =====================================

CINet_Socket::CINet_Socket(NFile_Open_Mode mode) : IFile(NFile_Type_Major::Network_Socket), mode(mode) {  }

bool CINet_Socket::Compare_Incoming_Source(const TIP_Address_Container& in_src) const {
	if (!(flags & static_cast<uint8_t>(NINet_Socket_Flags::CONNECTED))) 
		return true; // If not connected then comparison ignored
	
	return Compare_Addresses(remote_addr, in_src);
}

bool CINet_Socket::Compare_Incoming_Destination(const TIP_Address_Container& in_dst) const {
	return Compare_Addresses(local_addr, in_dst);
}

NINet_Socket_Errors CINet_Socket::Configure(const NInternet_Protocol internet, const NTransport_Protocol transport) {
	if (flags & static_cast<uint8_t>(NINet_Socket_Flags::CONFIGURED))
		return NINet_Socket_Errors::REPEATED_CALL; // Overwriting configuration is not permitted
	
	ip = internet;
	tp = transport;
	flags |= static_cast<uint8_t>(NINet_Socket_Flags::CONFIGURED);
	return NINet_Socket_Errors::OK;
}

NINet_Socket_Errors CINet_Socket::Bind(const TIP_Address_Container& local) {
	if (!(flags & static_cast<uint8_t>(NINet_Socket_Flags::CONFIGURED)))
		return NINet_Socket_Errors::BIND_FAIL; // Cannot bind if socket not configured
	
	if (local._ip != ip || sINet_Socket_Provider.Get_Socket(local, tp))
		return NINet_Socket_Errors::REPEATED_CALL; // Overriding binding is not permitted

	local_addr = local;
	flags |= static_cast<uint8_t>(NINet_Socket_Flags::BOUND);
	return NINet_Socket_Errors::OK;
}

NINet_Socket_Errors CINet_Socket::Connect(const TIP_Address_Container& remote) {
	if (!(flags & static_cast<uint8_t>(NINet_Socket_Flags::BOUND)))
		return NINet_Socket_Errors::CONNECT_FAIL; // Cannot connect if socket not bound

	if (remote._ip != ip)
		return NINet_Socket_Errors::CONNECT_FAIL; // Need to bind to same address

	remote_addr = remote;
	flags |= static_cast<uint8_t>(NINet_Socket_Flags::CONNECTED);
	return NINet_Socket_Errors::OK;
}

uint32_t CINet_Socket::Send(const char* buff, const uint32_t num) {
	if (!(flags & static_cast<uint8_t>(NINet_Socket_Flags::CONNECTED)))
		return INET_SOCKET_ERROR;

	return Send_To(buff, num, remote_addr);
}

uint32_t CINet_Socket::Send_To(const char* buff, const uint32_t num, TIP_Address_Container& addr) {
	if (mode == NFile_Open_Mode::Read_Only)
		return INET_SOCKET_ERROR;

	return 0;
}

uint32_t CINet_Socket::Recv(char* buff, const uint32_t num) {
	if (!(flags & static_cast<uint8_t>(NINet_Socket_Flags::CONNECTED)))
		return INET_SOCKET_ERROR;

	return Recv_From(buff, num, remote_addr);
}

uint32_t CINet_Socket::Recv_From(char* buff, const uint32_t num, TIP_Address_Container& addr) {
	if (mode == NFile_Open_Mode::Write_Only)
		return INET_SOCKET_ERROR;

	return 0;
}

uint8_t CINet_Socket::Flags() const { return flags; }

bool CINet_Socket::Compare_Local_Bind(const TIP_Address_Container& addr, const NTransport_Protocol tpv) const {
	return Compare_Addresses(local_addr, addr) && ip == addr._ip && tp == tpv;
}

uint32_t CINet_Socket::Read(char* buffer, uint32_t num) { return Recv(buffer, num); }

uint32_t CINet_Socket::Write(const char* buffer, uint32_t num) { return Send(buffer, num); }

bool CINet_Socket::Close() {
	flags |= static_cast<uint8_t>(NINet_Socket_Flags::CLOSED);
	sINet_Socket_Provider.Unregister_Socket(this);
	return true;
}

// ============================ Socket Provider ===================================

CINet_Socket_Provider::CINet_Socket_Provider() {
	for (auto i = 0; i < INET_MAX_REGISTERED_SOCKETS; i++) {
		bound[i] = nullptr;
	}

	provider_mtx = mutex_create("socket_mtx");
	sock_count = 0;
}

CINet_Socket_Provider::~CINet_Socket_Provider() {
	mutex_destroy(provider_mtx);
}

bool CINet_Socket_Provider::Register_Socket(CINet_Socket* socket) {
	if (sock_count == INET_MAX_REGISTERED_SOCKETS)
		return false;

	while (!mutex_lock(provider_mtx)) {
		; /* Active wait until mutex acquired */
	}

	for (auto i = 0; i < INET_MAX_REGISTERED_SOCKETS; i++) {
		if (!bound[i]) {
			bound[i] = socket;
			break;
		}
	}

	sock_count++;
	mutex_unlock(provider_mtx);
	return true;
}

bool CINet_Socket_Provider::Unregister_Socket(CINet_Socket* socket_addr) {			
	while (!mutex_lock(provider_mtx)) {
		; /* Active wait until mutex acquired */
	}
	
	for (auto i = 0; i < INET_MAX_REGISTERED_SOCKETS; i++) {
		if (bound[i] == socket_addr) {
			bound[i] = nullptr;
			sock_count--;
			return true;
		}
	}

	mutex_unlock(provider_mtx);
	return false;
}

bool CINet_Socket_Provider::Contains(const CINet_Socket* socket) const {
	if (!socket)
		return false;

	for (const auto sock : bound) {
		if (sock == socket)
			return true;
	}

	return false;
}

CINet_Socket* CINet_Socket_Provider::Get_Socket(const TIP_Address_Container& address, const NTransport_Protocol tp) const {
	for (auto i = 0; i < INET_MAX_REGISTERED_SOCKETS; i++) {
		if (bound[i] && bound[i]->Compare_Local_Bind(address, tp))
			return bound[i];
	}

	return nullptr;
}

CINet_Socket_Provider sINet_Socket_Provider{};

// ============================= INet Filesystem ============================

bool Parse_IP(const char* str, uint32_t& end, NInternet_Protocol& ip) {
	if (!str)
		return false;
	
	constexpr int TMPS = 3;
	char tmp[TMPS];
	int eix = 0;
	end = 0;

	for ( ; str[end] != '/' && str[end] != '\0'; end++) {
		;
	}

	eix = static_cast<int>(end > TMPS) ? TMPS : end;
	strncpy(tmp, str, eix);
	tmp[eix - 1] = '\0';

	ip = static_cast<NInternet_Protocol>(atoi(tmp));
	return true;
}

bool Parse_TP(const char* str, uint32_t& end, NTransport_Protocol& tp) {
	if (!str)
		return false;

	constexpr int TMPS = 5;
	char tmp[TMPS];
	int eix = 0;
	end = 0;

	for ( ; str[end] != '/' && str[end] != '\0'; end++) {
		;
	}

	eix = static_cast<int>(end > TMPS) ? TMPS : end;
	strncpy(tmp, str, eix);
	tmp[eix - 1] = '\0';

	for (const auto& tpc : transport_protos) {
		if (strncmp(tmp, tpc.name, eix) == 0) {
			tp = tpc.proto;
			return true;
		}
	}

	return false;
}

bool Parse_Port(const char* str, uint32_t& end, TIP_Address_Container& c) {
	if (!str)
		return false;

	constexpr int TMPS = 6;
	char tmp[TMPS];
	int eix = 0;
	end = 0;

	for ( ; str[end] != '/' && str[end] != '\0'; end++) {
		;
	}

	eix = static_cast<int>(end > TMPS) ? TMPS : end;
	strncpy(tmp, str, eix);
	tmp[eix - 1] = '\0';

	c.port = static_cast<TNetwork_Port>(atoi(tmp));
	return true;
}

bool Parse_Addr(const char* str, uint32_t& end, TIP_Address_Container& c, const NInternet_Protocol ip) {
	if (!str)
		return false;

	constexpr int TMPS = 16;
	char tmp[TMPS];
	int eix = 0;
	end = 0;

	for ( ; str[end] != '/' && str[end] != '\0'; end++) {
		;
	}

	eix = static_cast<int>(end > TMPS) ? TMPS : end;
	strncpy(tmp, str, eix);
	tmp[eix - 1] = '\0';

	return _parse_addr_str(tmp, ip, c);
}