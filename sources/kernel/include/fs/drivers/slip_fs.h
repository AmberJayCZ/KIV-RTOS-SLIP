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
			if (slip_driver.Get_Socket(local, tp))
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