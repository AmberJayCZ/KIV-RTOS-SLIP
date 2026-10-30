#include <fs/drivers/inet_fs.h>
#include <drivers/uart.h>

constexpr auto DRIVER_BUFFER_SIZE = 1 << 14;

/* SLIP socket class */
class CSLIP_Socket : public CINet_Socket {
	public:
		CSLIP_Socket(NFile_Open_Mode mode) : CINet_Socket(mode) {  }

		~CSLIP_Socket() override;

		uint32_t Send(const char* buff, const uint32_t num) override;

		uint32_t Send_To(const char* buff, const uint32_t num, TIP_Address_Container* addr) override;

		uint32_t Recv(char* buff, const uint32_t num) override;

		uint32_t Recv_From(char* buff, const uint32_t num, TIP_Address_Container* addr) override;



		// IFile implementation - all methods for future reference

		uint32_t Read(char* buffer, uint32_t num) override;

		uint32_t Write(const char* buffer, uint32_t num) override;

		bool Close() override;

		bool IOCtl(NIOCtl_Operation dir, void* ctlptr) override;

		bool Wait(uint32_t count) override;

		bool Try_Wait_All_Reserve(uint32_t count) override;

		bool Wait_All_Acquire(uint32_t count) override;

		uint32_t Notify(uint32_t count) override;

		void Wait_Enqueue_Current() override;

		bool Wait_Dequeue_Process(uint32_t pid) override;
};

/* SLIP driver class */
class CSLIP_Driver : public CINet_Driver {
	private:
		char inbound[DRIVER_BUFFER_SIZE];
		char outbound[DRIVER_BUFFER_SIZE];
		CSLIP_Socket* bound[SLIP_MAX_BOUND_SOCKETS];

	public:
		CSLIP_Driver();

		CSLIP_Socket* Get_Socket(const TIP_Address_Container& address, const NTransport_Protocol protocol);

		void On_Register() override;

		IFile* Open_File(const char* path, NFile_Open_Mode mode) override;
};