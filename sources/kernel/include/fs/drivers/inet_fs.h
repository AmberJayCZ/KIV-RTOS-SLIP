#include <drivers/bridges/inet_defs.h>
#include <drivers/inet.h>
#include <fs/filesystem.h>
#include <stdmutex.h>

constexpr uint32_t INET_SOCKET_ERROR = 0xFFFFFFFF; /* IFile calls on a TINet_Socket that return this value were erroneous */
constexpr uint32_t INET_MAX_REGISTERED_SOCKETS = 10;

constexpr char* INET_IP4 = "4";
constexpr char* INET_UDP = "UDP";

struct INet_TP_Container {
	const char* name;
	NTransport_Protocol proto;
};

const INet_TP_Container transport_protos[] = {
	{ INET_UDP, NTransport_Protocol::UDP }
};

/* Network socket status flags */
enum class NINet_Socket_Flags : uint8_t {
	CLOSED 		= 1,	/* Socket is closed */
	CONFIGURED	= 2,	/* Socket is configured */
	BOUND 		= 4,	/* Socket is bound to local interface */
	CONNECTED	= 8,	/* Socket is connected to remote host */

	IN_FULL 	= 32,	/* Input buffer is full - TODO review if needed */
	OUT_FULL 	= 64,	/* Output buffer is full - TODO review if needed */
	ERROR		= 128,	/* Last operation caused an error */
};

/* Network socket error codes */
enum class NINet_Socket_Errors : uint32_t {
	OK = 0,				/* No errors */
	CONFIGURE_FAIL,		/* Configuration of socket failed */
	BIND_FAIL,			/* Binding to local interface failed */
	CONNECT_FAIL,		/* Connecting to remote endpoint failed */
	SEND_BUFF_OVERFLOW,	/* No write was performed - not enough space in outbound buffer - TODO review if needed */
	RECV_BUFF_OVERFLOW,	/* Inbound buffer overflowed - TODO review if needed */
};

/* Network socket class */
class CINet_Socket : public IFile {
	protected:
		NInternet_Protocol ip;
		NTransport_Protocol tp;
		NFile_Open_Mode mode;
		TIP_Address_Container local_addr, remote_addr; // Container for arbitrary raw data, used with reinterpret_cast and instance data to ascertain actual address type
		uint8_t flags;
		NINet_Socket_Errors error;

	public:
		CINet_Socket(NFile_Open_Mode mode) : IFile(NFile_Type_Major::Network_Socket), mode(mode) {  }

		virtual ~CINet_Socket();

		virtual NINet_Socket_Errors Configure(const NInternet_Protocol internet, const NTransport_Protocol transport) {
			ip = internet;
			tp = transport;
			flags |= static_cast<uint8_t>(NINet_Socket_Flags::CONFIGURED);
			return NINet_Socket_Errors::OK;
		}

		virtual NINet_Socket_Errors Bind(const TIP_Address_Container& local) {
			if (socket_provider.Get_Socket(local, tp))
				return NINet_Socket_Errors::BIND_FAIL;

			local_addr = local;
			flags |= static_cast<uint8_t>(NINet_Socket_Flags::BOUND);
			return NINet_Socket_Errors::OK;
		}

		virtual NINet_Socket_Errors Connect(const TIP_Address_Container& remote) {
			remote_addr = remote;
			flags |= static_cast<uint8_t>(NINet_Socket_Flags::CONNECTED);
			return NINet_Socket_Errors::OK;
		}

		virtual uint32_t Send(const char* buff, const uint32_t num) {
			if (mode == NFile_Open_Mode::Read_Only)
				return INET_SOCKET_ERROR;

			if (!(flags & static_cast<uint8_t>(NINet_Socket_Flags::CONNECTED)))
				return INET_SOCKET_ERROR;

			return 0;
		}

		virtual uint32_t Send_To(const char* buff, const uint32_t num, TIP_Address_Container* addr) {
			if (mode == NFile_Open_Mode::Read_Only)
				return INET_SOCKET_ERROR;

			return 0;
		}

		virtual uint32_t Recv(char* buff, const uint32_t num) {
			if (mode == NFile_Open_Mode::Write_Only)
				return INET_SOCKET_ERROR;

			if (!(flags & static_cast<uint8_t>(NINet_Socket_Flags::CONNECTED)))
				return INET_SOCKET_ERROR;

			return 0;
		}

		virtual uint32_t Recv_From(char* buff, const uint32_t num, TIP_Address_Container* addr) {
			if (mode == NFile_Open_Mode::Write_Only)
				return INET_SOCKET_ERROR;

			return 0;
		}

		uint8_t Flags() const { return flags; }

		bool Compare_Local_Bind(const TIP_Address_Container& addr, const NTransport_Protocol protocol) const {
			return Compare_Addresses(local_addr, addr, ip) && tp == protocol;
		}

		// IFile implementation - all methods for future reference - TODO review which to implement / override

		virtual uint32_t Read(char* buffer, uint32_t num) override { return Recv(buffer, num); }

		virtual uint32_t Write(const char* buffer, uint32_t num) override { return Send(buffer, num); }

		virtual bool Close() override {
			flags |= static_cast<uint8_t>(NINet_Socket_Flags::CLOSED);
			socket_provider.Unregister_Socket(this);
			return true;
		}

		virtual bool IOCtl(NIOCtl_Operation dir, void* ctlptr);

		virtual bool Wait(uint32_t count);

		virtual bool Try_Wait_All_Reserve(uint32_t count);

		virtual bool Wait_All_Acquire(uint32_t count);

		virtual uint32_t Notify(uint32_t count);

		virtual void Wait_Enqueue_Current();

		virtual bool Wait_Dequeue_Process(uint32_t pid);
};



class CINet_Socket_Provider {
	private:
		CINet_Socket* bound[INET_MAX_REGISTERED_SOCKETS];
		uint32_t sock_count;
		mutex_t provider_mtx;

	public:
		CINet_Socket_Provider() {
			for (auto& sock : bound) {
				sock = nullptr;
			}

			provider_mtx = mutex_create("socket_mtx");
			sock_count = 0;
		}

		~CINet_Socket_Provider() {
			mutex_destroy(provider_mtx);
		}

		bool Register_Socket(CINet_Socket* socket) {
			if (sock_count == INET_MAX_REGISTERED_SOCKETS)
				return false;

			while (!mutex_lock(provider_mtx)) {
				; /* Active wait until mutex acquired */
			}

			for (auto& sock : bound) {
				if (!sock) {
					sock = socket;
					break;
				}
			}

			sock_count++;
			mutex_unlock(provider_mtx);
			return true;
		}

		bool Unregister_Socket(CINet_Socket* socket_addr) {			
			while (!mutex_lock(provider_mtx)) {
				; /* Active wait until mutex acquired */
			}
			
			for (auto& sock : bound) {
				if (sock == socket_addr) {
					sock = nullptr;
					sock_count--;
					return true;
				}
			}

			mutex_unlock(provider_mtx);
			return false;
		}

		bool Contains(const CINet_Socket* socket) const {
			if (!socket)
				return false;

			for (const auto sock : bound) {
				if (sock == socket)
					return true;
			}

			return false;
		}

		CINet_Socket* Get_Socket(const TIP_Address_Container& address, const NTransport_Protocol protocol) const {
			for (auto sock : bound) {
				if (sock && sock->Compare_Local_Bind(address, protocol))
					return sock;
			}

			return nullptr;
		}
};

extern CINet_Socket_Provider socket_provider;