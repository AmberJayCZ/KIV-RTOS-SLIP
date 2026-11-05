#include <fs/drivers/inet_fs.h>
#include <drivers/uart.h>
#include <circbuffer.h>
#include <stdmutex.h>

constexpr uint32_t SLIP_MAX_BOUND_SOCKETS = 10;
constexpr auto SLIP_DRIVER_OUTPUT_BUFF_SIZE = 1 << 14;
constexpr auto SLIP_SOCKET_INPUT_BUFF_SIZE = 1 << 12;
constexpr auto SLIP_TRANSFORM_BUFF_SIZE = 1 << 10;

constexpr char END = 192;
constexpr char ESC = 219;
constexpr char ESC_END = 220;
constexpr char ESC_ESC = 221;

/* SLIP socket class */
class CSLIP_Socket : public CINet_Socket {
	private:
		char send_transform_buff[SLIP_TRANSFORM_BUFF_SIZE];
		NonBlocking_Circular_Buffer_Queue<char, SLIP_SOCKET_INPUT_BUFF_SIZE> recv_buff;

	public:
		CSLIP_Socket(NFile_Open_Mode mode) : CINet_Socket(mode) {  }

		~CSLIP_Socket() override;

		NINet_Socket_Errors Bind(const TIP_Address_Container& local) override {
			if (slip_driver.Get_Socket(local, ip, tp))
				return NINet_Socket_Errors::BIND_FAIL;

			if (CINet_Socket::Bind(local) != NINet_Socket_Errors::OK)
				return NINet_Socket_Errors::BIND_FAIL;

			if (!slip_driver.Register_Socket(this))
				return NINet_Socket_Errors::BIND_FAIL;

			return NINet_Socket_Errors::OK;
		}

		uint32_t Send(const char* buff, const uint32_t num) override;

		uint32_t Send_To(const char* buff, const uint32_t num, TIP_Address_Container* addr) override;

		uint32_t Recv(char* buff, const uint32_t num) override;

		uint32_t Recv_From(char* buff, const uint32_t num, TIP_Address_Container* addr) override;

		/* Driver uses this to write into recv_buff */
		uint32_t Buffer_Write(const char* buff, const uint32_t num);



		// IFile implementation - all methods for future reference

		uint32_t Read(char* buffer, uint32_t num) override;

		uint32_t Write(const char* buffer, uint32_t num) override;

		bool Close() override {
			CINet_Socket::Close();
			return slip_driver.Unregister_Socket(this);
		}

		bool IOCtl(NIOCtl_Operation dir, void* ctlptr) override;

		bool Wait(uint32_t count) override;

		bool Try_Wait_All_Reserve(uint32_t count) override;

		bool Wait_All_Acquire(uint32_t count) override;

		uint32_t Notify(uint32_t count) override;

		void Wait_Enqueue_Current() override;

		bool Wait_Dequeue_Process(uint32_t pid) override;
};

/* SLIP driver class */
class CSLIP_Driver : public IFilesystem_Driver {
	private:
		CUART& uart;
		NonBlocking_Circular_Buffer_Queue<char, SLIP_DRIVER_OUTPUT_BUFF_SIZE> send_buff;
		char recv_transform_buff[SLIP_TRANSFORM_BUFF_SIZE];
		uint32_t partial_recv_idx;
		TIP_Address_Container partial_recv_addr;
		mutex_t driver_mtx;
		CSLIP_Socket* bound[SLIP_MAX_BOUND_SOCKETS];
		uint32_t sock_count;

	public:
		CSLIP_Driver(CUART& uart) : uart(uart) {
			driver_mtx = mutex_create("slip_mtx");

			for (auto& sock : bound) {
				sock = nullptr;
			}
		}

		~CSLIP_Driver() {
			mutex_destroy(driver_mtx);
		}

		bool Register_Socket(CSLIP_Socket* socket) {
			if (sock_count == SLIP_MAX_BOUND_SOCKETS) /* Socket field is full */
				return false;

			while (!mutex_lock(driver_mtx)) {
				; /* Active wait until mutex acquired */
			}

			for (auto& sock : bound) {
				if (!sock) {
					sock = socket;
					break;
				}
			}

			sock_count++;
			mutex_unlock(driver_mtx);
			return true;
		}

		bool Unregister_Socket(CSLIP_Socket* socket_addr) {
			while (!mutex_lock(driver_mtx)) {
				; /* Active wait until mutex acquired */
			}

			for (auto& sock : bound) {
				if (sock == socket_addr) {
					sock = nullptr;
					sock_count--;
					mutex_unlock(driver_mtx);
					return true;
				}
			}

			mutex_unlock(driver_mtx);
			return false;
		}

		CSLIP_Socket* Get_Socket(const TIP_Address_Container& address, const NInternet_Protocol ipv, const NTransport_Protocol tpv) const {
			for (auto& sock : bound) {
				if (sock && sock->Compare_Local_Bind(address, ipv, tpv))
					return sock;
			}

			return nullptr;
		}

		void Send_Message(const char* msg, const uint32_t size) {
			while (!mutex_lock(driver_mtx)) {
				; /* Active wait until mutex acquired */
			}

			uint32_t sent = 0;

			while (sent != size) {
				sent += send_buff.Multi_Put(&msg[sent], size - sent); /* Write whole message into buffer */
			}

			mutex_unlock(driver_mtx); /* Release mutex once data is copied into buffer */
		}

		void Run_Driver() {
			/* TODO */
		}

		// IFS_Driver interface methods

		void On_Register() override {
			/* TODO - if used */
		}

		IFile* Open_File(const char* path, NFile_Open_Mode mode) override {
			/* TODO */
		}
};

extern CSLIP_Driver slip_driver;


// TODO - testing
/* Encodes at most max(data_len, buffer_len - 1) characters into buffer in accordance with SLIP encoding. Returns number of written bytes. Parameter end_position is index in data stream where encoding was stopped and end_tag is true if END was appended to message. If end_position < data_len or !end_tag then buffer needs to be flushed and encoding continued from end_position. Note that end_position and end_tag are reset upon calling to 0 and false respectively. */
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

// TODO - testing
/* Decodes at most max(data_len - 1, buffer_len) characters from data into buffer, returns number of decoded characters in buffer. Parameter end_position is index in data stream where decoding was stopped and end_tag is set if data[end_position] is END. If end_position < data_len or !end_tag then message was not decoded entirely, buffer needs to be flushed and decoding continued from end_position. Note that end_position and end_tag are reset upon calling to 0 and false respectively. */
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