#include "socket.h"
#include "windows_socket.h"

unique_ptr<IListenSocket> Sockets::CreateListenSocket()
{
#ifdef WIN32
	return make_unique<windows_listen_socket>();
#elif __linux__
	return make_unique<linux_listen_socket>();
#endif
}

unique_ptr<ISenderSocket> Sockets::CreateSenderSocket(string peer_ip)
{
#ifdef WIN32
	return make_unique<windows_send_socket>(peer_ip);
#elif __linux__
	return make_unique<linux_send_socket>(peer_ip);
#endif
}
