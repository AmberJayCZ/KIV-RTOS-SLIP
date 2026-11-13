#include <fs/drivers/slip_fs.h>
#include <fs/drivers/uart_fs.h>

constexpr char END = 192;
constexpr char ESC = 219;
constexpr char ESC_END = 220;
constexpr char ESC_ESC = 221;

// ===================== SLIP functions =============================

uint32_t SLIP_Encode(const char* data, const uint32_t data_len, char* buffer, const uint32_t buffer_len, uint32_t& end_position, bool& end_tag) {
	uint32_t written = 0;

	for (end_position = 0, end_tag = false; end_position < data_len && written < buffer_len - 1; end_position++) {
		if (data[end_position] == END) {
			buffer[written++] = ESC;
			buffer[written++] = ESC_END; /* writing is limited to buffer_len - 1 because of these doubled tags */
		}

		else if (data[end_position] == ESC) {
			buffer[written++] = ESC;
			buffer[written++] = ESC_ESC;
		}

		else {
			buffer[written++] = data[end_position];
		}
	}

	if (end_position == data_len && written < buffer_len) {
		buffer[written++] = END; /* Write END tag only if there is space left */
		end_tag = true;
	}

	return written;
}

uint32_t SLIP_Decode(const char* data, const uint32_t data_len, char* buffer, const uint32_t buffer_len, uint32_t& end_position, bool& end_tag) {
	uint32_t decoded = 0;

	for (end_position = 0, end_tag = false; end_position < data_len - 1 && decoded < buffer_len && data[end_position] != END; end_position++) {
		if (data[end_position] == ESC && data[end_position + 1] == ESC_END) {
			buffer[decoded++] = END;
			end_position++; /* Increment end_position by 2 because 2 chars were processed */
		}

		else if (data[end_position] == ESC && data[end_position + 1] == ESC_ESC) {
			buffer[decoded++] = ESC;
			end_position++; /* Increment end_position by 2 because 2 chars were processed */
		}

		else {
			buffer[decoded++] = data[end_position];
		}
	}

	if (end_position < data_len && data[end_position] == END) {
		end_tag = true; /* END tag detected */
	}

	return decoded;
}

// ===================== SLIP socket ================================

CSLIP_Socket::CSLIP_Socket(NFile_Open_Mode mode) : CINet_Socket(mode) {  }

// ~CSLIP_Socket() override;

NINet_Socket_Errors CSLIP_Socket::Bind(const TIP_Address_Container& local) {
	if (fsSLIP_Driver.Get_Socket(local, tp))
		return NINet_Socket_Errors::BIND_FAIL;

	if (CINet_Socket::Bind(local) != NINet_Socket_Errors::OK)
		return NINet_Socket_Errors::BIND_FAIL;

	if (!fsSLIP_Driver.Register_Socket(this))
		return NINet_Socket_Errors::BIND_FAIL;

	return NINet_Socket_Errors::OK;
}

uint32_t CSLIP_Socket::Send(const char* buff, const uint32_t num) {
	if (!(flags & static_cast<uint8_t>(NINet_Socket_Flags::CONNECTED)))
		return INET_SOCKET_ERROR;

	return Send_To(buff, num, remote_addr);
}

// TODO: TEST
uint32_t CSLIP_Socket::Send_To(const char* buff, const uint32_t num, TIP_Address_Container& addr) {
	if (mode == NFile_Open_Mode::Read_Only)
		return INET_SOCKET_ERROR;

	uint32_t sent = 0;
	uint32_t endpos = 0;
	bool finished = false;

	const auto header_len = _make_headers(send_transform_buff, ip, tp, local_addr, remote_addr, num);
	const auto header_enc = SLIP_Encode(send_transform_buff, header_len, &send_transform_buff[header_len], SLIP_TB_SIZE - header_len, endpos, finished);
	// We assume that headers and their encoded copies can fit into a 1KiB buffer

	auto msg_enc = SLIP_Encode(buff, num, &send_transform_buff[header_len + header_enc - 1], SLIP_TB_SIZE - (header_len + header_enc - 1), endpos, finished);
	// -1 because transform_buff[header_len + header_enc] is END tag from header encoding, which we overwrite
	uint32_t str_idx = endpos;

	// Get exclusive access to output queue
	fsSLIP_Driver.Lock_Buffer();

	sent += fsSLIP_Driver.Send_Message(&send_transform_buff[header_len], msg_enc + header_enc - 1);
	// -1 because header_enc includes the END tag we overwrote

	// If encoding was not completed, we encode and send the rest of the message
	while (!finished) {
		msg_enc = SLIP_Encode(&buff[str_idx], num - str_idx, send_transform_buff, SLIP_TB_SIZE, endpos, finished);
		str_idx += endpos;
		sent += fsSLIP_Driver.Send_Message(send_transform_buff, msg_enc);
		
	}

	// Release access to buffer
	fsSLIP_Driver.Unlock_Buffer();
	return sent;
}

uint32_t CSLIP_Socket::Recv(char* buff, const uint32_t num) {
	if (!(flags & static_cast<uint8_t>(NINet_Socket_Flags::CONNECTED)))
		return INET_SOCKET_ERROR;

	return Recv_From(buff, num, remote_addr);
}

// TODO: Test
uint32_t CSLIP_Socket::Recv_From(char* buff, const uint32_t num, TIP_Address_Container& addr) {
	if (mode == NFile_Open_Mode::Write_Only)
		return INET_SOCKET_ERROR;

	if (recv_buff.Empty())
		return 0;

	uint32_t read = 0;
	constexpr auto TMP_B_SIZE = 100;
	char header[TMP_B_SIZE];
	NInternet_Protocol in_ip = NInternet_Protocol::V4;
	NTransport_Protocol in_tp = NTransport_Protocol::UDP;

	recv_buff.Multi_Remove(header, 20); // Expects at least 20 bytes (minimum IPv4 header length)
	uint32_t iph_len = reinterpret_cast<TIP_Header_Pretext*>(header)->IHL * 4;
	uint32_t data_len = 0;
	recv_buff.Multi_Remove(&header[20], iph_len); // Get rest of IP header

	// Extract IP addresses
	auto in_src = Get_Addr_Container(ip);
	auto in_dst = Get_Addr_Container(ip);
	
	if (!_parse_ip_header(header, in_ip, in_tp, in_src, in_dst, iph_len, data_len)) return 0;

	auto tph_len = _get_header_length(in_tp);
	recv_buff.Multi_Remove(&header[iph_len], tph_len);

	if (!_parse_tp_header(&header[iph_len], in_tp, in_src, in_dst, tph_len, data_len)) return 0;

	const auto msg_len = data_len - tph_len; // Length of actual payload

	const auto headers_valid = _verify_header_checksums(header, in_ip, iph_len, in_tp);
	
	const auto src_match = Compare_Incoming_Source(in_src) && Compare_Addresses(addr, in_src);
	
	if (headers_valid && src_match) {
		addr = in_src; // Copy actual source address into addr
		return recv_buff.Multi_Remove(buff, num);
	}

	else { // Discard rest of message
		while (read != msg_len) {
			const auto to_remove = msg_len - read;
			read += recv_buff.Multi_Remove(header, (to_remove > TMP_B_SIZE) ? TMP_B_SIZE : to_remove);
		}

		return 0;
	}
}

/* Driver uses this to write into recv_buff */
uint32_t CSLIP_Socket::Buffer_Write(const char* buff, const uint32_t num) {
	if (num > recv_buff.Space())
		return 0; /* No data written if it would not fit */

	return recv_buff.Multi_Put(buff, num);
}



// IFile implementation - all methods for future reference

// uint32_t Read(char* buffer, uint32_t num) override;

// uint32_t Write(const char* buffer, uint32_t num) override;

bool CSLIP_Socket::Close() {
	CINet_Socket::Close();
	return fsSLIP_Driver.Unregister_Socket(this);
}

// bool IOCtl(NIOCtl_Operation dir, void* ctlptr) override;

// bool Wait(uint32_t count) override;

// bool Try_Wait_All_Reserve(uint32_t count) override;

// bool Wait_All_Acquire(uint32_t count) override;

// uint32_t Notify(uint32_t count) override;

// void Wait_Enqueue_Current() override;

// bool Wait_Dequeue_Process(uint32_t pid) override;

// ========================================= SLIP Driver ======================================

CSLIP_Driver::CSLIP_Driver() {
	// Create mutex
	driver_mtx = mutex_create("slip_mtx");

	// Initialize bounding array
	for (auto i = 0; i < SLIP_MAX_BOUND_SOCKETS; i++) {
		bound[i] = nullptr;
	}
}

CSLIP_Driver::~CSLIP_Driver() {
	mutex_destroy(driver_mtx);
}

bool CSLIP_Driver::Register_Socket(CSLIP_Socket* socket) {
	if (sock_count == SLIP_MAX_BOUND_SOCKETS) /* Socket field is full */
		return false;

	while (!mutex_lock(driver_mtx)) {
		; /* Active wait until mutex acquired */
	}

	for (auto i = 0; i < INET_MAX_REGISTERED_SOCKETS; i++) {
		if (!bound[i]) {
			bound[i] = socket;
			break;
		}
	}

	sock_count++;
	mutex_unlock(driver_mtx);
	return true;
}

bool CSLIP_Driver::Unregister_Socket(CSLIP_Socket* socket_addr) {
	while (!mutex_lock(driver_mtx)) {
		; /* Active wait until mutex acquired */
	}

	for (auto i = 0; i < SLIP_MAX_BOUND_SOCKETS; i++) {
		if (bound[i] == socket_addr) {
			bound[i] = nullptr;
			sock_count--;
			mutex_unlock(driver_mtx);
			return true;
		}
	}

	mutex_unlock(driver_mtx);
	return false;
}

CSLIP_Socket* CSLIP_Driver::Get_Socket(const TIP_Address_Container& address, const NTransport_Protocol tpv) const {
	for (auto i = 0; i < SLIP_MAX_BOUND_SOCKETS; i++) {
		if (bound[i] && bound[i]->Compare_Local_Bind(address, tpv))
			return bound[i];
	}

	return nullptr;
}

void CSLIP_Driver::Lock_Buffer() {
	while (!mutex_lock(driver_mtx)) {
		; /* Active wait until mutex acquired */
	}
}

uint32_t CSLIP_Driver::Send_Message(const char* msg, const uint32_t size) {
	uint32_t sent = 0;

	// Write complete message into buffer in parts if need be
	while (sent != size) {
		sent += send_buff.Multi_Put(&msg[sent], size - sent);
	}

	return sent;
}

void CSLIP_Driver::Unlock_Buffer() {
	mutex_unlock(driver_mtx);
}

void CSLIP_Driver::Run_Driver() {
	bool ignore = false;	// Ignoring incoming data
	uint32_t read = 0;		// Number of bytes read from UART buffer
	uint32_t send = 0;		// Number of outcoming bytes to send
	uint32_t partial_recv_idx = 0;	// Index of end of partial message in buffer (i. e. "append" position to continue decoding)
	
	while (true) {
		read = uart_file->Read(transform_buff, SLIP_TB_DECD_SIZE);

		if (read > 0) {
			uint32_t processed = 0;	// Number of recieved bytes processed
			uint32_t end_pos = 0;	// Index in decoded data string where decoding stopped

			// TODO: Test address comparison on sockets and determine where the address should go when end tag reached
			// TODO: Validate checksums

			// We need to process all incoming data
			while (processed != read) {
				bool end_tag = false;	// End tag present in incoming data
				uint32_t decoded = 0;	// Number of bytes decoded from incoming data

				// Ignoring incoming data
				while (ignore && processed != read) {
					SLIP_Decode(transform_buff, read - processed, &transform_buff[SLIP_TB_DECD_SIZE], SLIP_TB_RECV_SIZE, end_pos, end_tag);
					processed += end_pos; // Endpos is position in string where decoding stopped
					ignore = end_tag == true; // Stop ignoring if end of message is reached

					if (!ignore)
						processed++; // If ignoring stopped => trans_buff[processed] would be END tag, so we skip it
				}

				// Processing until end of message, data limit or buffer limit
				while (!end_tag && processed != read) {
					decoded += SLIP_Decode(&transform_buff[processed], read - processed, &transform_buff[SLIP_TB_DECD_SIZE + decoded], SLIP_TB_RECV_SIZE - decoded, end_pos, end_tag); // Decode from last position
					processed += end_pos; // Endpos is position in string where decoding stopped

					if (decoded >= SLIP_TB_RECV_SIZE && !end_tag) {
						ignore = true; // If we ran out of buffer space and message did not end, ignore rest
						break;
					}
				}

				// If message parsing completed, we process it
				if (end_tag) {
					processed++; // If end tag was found, then trans_buff[processed] would be END tag, so we skip it for subsequent decoding
					NInternet_Protocol ip = NInternet_Protocol::V4;			// Placeholder for referencing parsed IP
					NTransport_Protocol tp = NTransport_Protocol::UDP;		// Placeholder for referencing parsed TP
					TIP_Address_Container in_dst = Get_Addr_Container(ip);	// Container for destination address
					uint32_t ihl = reinterpret_cast<TIP_Header_Pretext*>(&transform_buff[SLIP_TB_DECD_SIZE])->IHL * 4;

					if (_verify_header_checksums(&transform_buff[SLIP_TB_DECD_SIZE], ip, ihl, tp)) {
						const auto thl = _get_header_length(tp);
						const auto msl = _get_data_length(&transform_buff[SLIP_TB_DECD_SIZE + ihl], tp);

						// Header lengths + payload length need to sum up to number of decoded charcters
						if (ihl + thl + msl == decoded) {
							// Iterate through sockets to find the target
							for (auto sock : bound) {
								if (sock && sock->Compare_Local_Bind(in_dst, tp)) {
									sock->Buffer_Write(&transform_buff[SLIP_TB_DECD_SIZE], decoded);
								}
							}
						}
					}
				}

				// Repeat until all recieved bytes are processed
			}
		}

		if (!send_buff.Empty()) {
			send = send_buff.Multi_Remove(transform_buff, SLIP_TB_DECD_SIZE);
			
			while (send != 0) {
				send -= uart_file->Write(transform_buff, send);
			}
		}
	}
}

void CSLIP_Driver::On_Register() {
	// Open UART file - requires free UART0
	uart_file = fsUART_FS_Driver.Open_File("0", NFile_Open_Mode::Read_Write);
	TUART_IOCtl_Params settings { NUART_Char_Length::Char_8, NUART_Baud_Rate::BR_115200, NUART_Blocking_State::Non_Blocking };
	uart_file->IOCtl(NIOCtl_Operation::Set_Params, &settings);
}

IFile* CSLIP_Driver::Open_File(const char* path, NFile_Open_Mode mode) {
	uint32_t end = 0;
	NInternet_Protocol ipv = NInternet_Protocol::V4;
	NTransport_Protocol tpv = NTransport_Protocol::UDP;
	TIP_Address_Container local;
	TIP_Address_Container remote;
	bool parsed_protocols = false;
	bool parsed_locals = false;
	bool parsed_remotes = false;
	bool tmp = false;

	// Parse protocols
	if (path[end] != '\0') {
		tmp = Parse_IP(path, end, ipv);

		// If parsing failed or end was reached, signal error
		if (!tmp || !path[end])
			return nullptr;

		tmp = Parse_TP(&path[end + 1], end, tpv);

		if (!tmp)
			return nullptr;

		parsed_protocols = true;
	}

	// Parse local endpoint
	if (parsed_protocols && path[end] != '\0') {
		tmp = Parse_Addr(path, end, local, ipv);

		// If parsing failed or end was reached, signal error
		if (!tmp || !path[end])
			return nullptr;

		tmp = Parse_Port(&path[end + 1], end, local);

		if (!tmp)
			return nullptr;

		local.port = HToN_Port(local.port);
		parsed_locals = true;
	}

	// Parse remote endpoint
	if (parsed_locals && path[end] != '\0') {
		tmp = Parse_Addr(path, end, remote, ipv);

		// If parsing failed or end was reached, signal error
		if (!tmp || !path[end])
			return nullptr;

		tmp = Parse_Port(&path[end + 1], end, remote);

		if (!tmp)
			return nullptr;

		remote.port = HToN_Port(remote.port);
		parsed_remotes = true;
	}

	CSLIP_Socket* socket = new CSLIP_Socket(mode);
	tmp = true;

	if (parsed_protocols) {
		tmp = socket->Configure(ipv, tpv) == NINet_Socket_Errors::OK;
	}

	if (tmp && parsed_locals) {
		tmp = socket->Bind(local) == NINet_Socket_Errors::OK;
	}

	if (tmp && parsed_remotes) {
		tmp = socket->Connect(remote) == NINet_Socket_Errors::OK;
	}

	if (!tmp) {
		socket->Close();
		// maybe call free on pointer?
		return nullptr;
	}

	return socket;
}

CSLIP_Driver fsSLIP_Driver{};

CUART_FS_Driver fsUART_FS_Driver{};