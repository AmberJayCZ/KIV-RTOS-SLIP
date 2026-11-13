#pragma once

#include <fs/drivers/inet_fs.h>
#include <circbuffer.h>
#include <stdmutex.h>

constexpr uint32_t SLIP_MAX_BOUND_SOCKETS = 10;	// Limit of sockets that SLIP driver can bind
constexpr auto SLIP_DR_OUT_BUFF_SIZE = 1 << 14;	// Size of SLIP driver output buffer
constexpr auto SLIP_SO_INP_BUFF_SIZE = 1 << 12;	// Size of SLIP socket input buffer
constexpr auto SLIP_TB_SIZE = 1 << 10;			// Size of SLIP transform buffer

constexpr auto SLIP_TB_DECD_SIZE = 24;	// Portion of SLIP transform buffer used for decoding recieved data
constexpr auto SLIP_TB_RECV_SIZE = SLIP_TB_SIZE - SLIP_TB_DECD_SIZE;	// Portion of transform buffer used to store decoded bytes



// TODO: testing
/* Encodes at most max(data_len, buffer_len - 1) characters into buffer in accordance with SLIP encoding. Returns number of written bytes. Parameter end_position is index in data stream where encoding was stopped and end_tag is true if END was appended to message. If end_position < data_len or !end_tag then buffer needs to be flushed and encoding continued from end_position. Note that end_position and end_tag are reset upon calling to 0 and false respectively. */
uint32_t SLIP_Encode(const char* data, const uint32_t data_len, char* buffer, const uint32_t buffer_len, uint32_t& end_position, bool& end_tag);

// TODO: testing
/* Decodes at most max(data_len - 1, buffer_len) characters from data into buffer, returns number of decoded characters in buffer. Parameter end_position is index in data stream where decoding was stopped and end_tag is set if data[end_position] is END. If end_position < data_len or !end_tag then message was not decoded entirely, buffer needs to be flushed and decoding continued from end_position. Note that end_position and end_tag are reset upon calling to 0 and false respectively. */
uint32_t SLIP_Decode(const char* data, const uint32_t data_len, char* buffer, const uint32_t buffer_len, uint32_t& end_position, bool& end_tag);



/* SLIP socket class */
class CSLIP_Socket : public CINet_Socket {
	private:
		char send_transform_buff[SLIP_TB_SIZE];
		NonBlocking_Circular_Buffer_Queue<char, SLIP_SO_INP_BUFF_SIZE> recv_buff;

	public:
		CSLIP_Socket(NFile_Open_Mode mode);

		// ~CSLIP_Socket() override;

		NINet_Socket_Errors Bind(const TIP_Address_Container& local) override;

		uint32_t Send(const char* buff, const uint32_t num) override;

		// TODO: TEST
		uint32_t Send_To(const char* buff, const uint32_t num, TIP_Address_Container& addr) override;

		uint32_t Recv(char* buff, const uint32_t num) override;

		// TODO: Test
		uint32_t Recv_From(char* buff, const uint32_t num, TIP_Address_Container& addr) override;

		/* Driver uses this to write into recv_buff */
		uint32_t Buffer_Write(const char* buff, const uint32_t num);



		// IFile implementation - all methods for future reference

		// uint32_t Read(char* buffer, uint32_t num) override;

		// uint32_t Write(const char* buffer, uint32_t num) override;

		bool Close();

		// bool IOCtl(NIOCtl_Operation dir, void* ctlptr) override;

		// bool Wait(uint32_t count) override;

		// bool Try_Wait_All_Reserve(uint32_t count) override;

		// bool Wait_All_Acquire(uint32_t count) override;

		// uint32_t Notify(uint32_t count) override;

		// void Wait_Enqueue_Current() override;

		// bool Wait_Dequeue_Process(uint32_t pid) override;
};

/* SLIP driver class */
class CSLIP_Driver : public IFilesystem_Driver {
	private:
		NonBlocking_Circular_Buffer_Queue<char, SLIP_DR_OUT_BUFF_SIZE> send_buff;
		char transform_buff[SLIP_TB_SIZE];
		mutex_t driver_mtx;
		CSLIP_Socket* bound[SLIP_MAX_BOUND_SOCKETS];
		uint32_t sock_count;
		IFile* uart_file;

	public:
		CSLIP_Driver();

		~CSLIP_Driver();

		// Registers socket in driver. Returns false if registration failed
		bool Register_Socket(CSLIP_Socket* socket);

		// Un-registers socket from driver. Returns false if socket was not found
		bool Unregister_Socket(CSLIP_Socket* socket_addr);

		// Gets socket by address and protocol. Returns nullptr if such socket was not found
		CSLIP_Socket* Get_Socket(const TIP_Address_Container& address, const NTransport_Protocol tpv) const;

		// Locks mutex
		void Lock_Buffer();

		// Writes size bytes from msg into output queue, requires exclusive access (i. e. prior Lock_Buffer() call and subsequent Unlock_Buffer() call)
		uint32_t Send_Message(const char* msg, const uint32_t size);

		// Unlocks mutex
		void Unlock_Buffer();

		// Main function
		void Run_Driver();

		// IFS_Driver interface methods

		void On_Register() override;

		IFile* Open_File(const char* path, NFile_Open_Mode mode) override;
};

// SLIP driver singleton
extern CSLIP_Driver fsSLIP_Driver;