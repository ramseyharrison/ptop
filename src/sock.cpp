#include "sock.h"

#if defined(WIN32)
	#ifndef WIN32_LEAN_AND_MEAN
		#define WIN32_LEAN_AND_MEAN
	#endif
	#include <windows.h>
	#include <winsock2.h>
	#include <ws2tcpip.h>
	#include <iphlpapi.h>
#elif defined(__linux__)
#include <sys/types.h> 
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <netdb.h>
#include <poll.h>
#include <errno.h>
#endif

#include <iostream>

epic_socket::epic_socket(protocol proto) : _protocol(proto)
{
	int domain = _protocol.get_ai_family();
	int type = _protocol.get_ai_socktype();
	int protocol = _protocol.get_ai_protocol();
	_handle = socket(domain, type, protocol);
}

epic_socket& epic_socket::bind_socket(const raw_name_data& name, std::string error_message)
{
	int result = bind(_handle, &name.name, name.name_len);
	throw_if_socket_error(result, error_message);
	return *this;
}

epic_socket& epic_socket::connect(sockaddr* addr, socklen_t len)
{
	if (_protocol.is_tcp())
	{
		int n = ::connect(_handle, addr, len);
		throw_if_socket_error(n, "Failed to connect");
	}
	else if (_protocol.is_udp())
	{
		_endpoint = raw_name_data(*addr, len);
	}
	return *this;
}

epic_socket& epic_socket::start_listening()
{
	if(_protocol.is_udp()) {
        std::cout << "UDP doesn't need a listen socket" << std::endl;
        return *this;
    }
	int n = ::listen(_handle, 4);
	throw_if_socket_error(n, "Failed to listen");
	return *this;
}

///returns a data socket using the listen socket
epic_socket epic_socket::accept_data_socket()
{
	sockaddr_in client_addr;
	socklen_t client_len;
	SOCKET new_socket = accept(_handle, (struct sockaddr*)&client_addr, &client_len);
	return epic_socket(new_socket, _protocol, raw_name_data(client_addr));
}

bool epic_socket::try_connect(sockaddr* addr, socklen_t len)
{
	int n = ::connect(_handle, addr, len);
	return n != SOCKET_ERROR;
}

void epic_socket::listen(int max_conns)
{
	auto n = ::listen(_handle, max_conns);
	throw_if_socket_error(n, "Failed to listen on socket");
}

bool epic_socket::has_connection() const
{
	struct timeval timeout;
	timeout.tv_sec = 0;
	timeout.tv_usec = 0;

	fd_set poll_read_set;
	FD_ZERO(&poll_read_set);
	FD_SET(_handle, &poll_read_set);

	int n = select(_handle + 1, &poll_read_set, 0, 0, &timeout);
	throw_if_socket_error(n, "Failed to poll socket readability");

	return n > 0;
}

bool epic_socket::poll_for(int poll_flag) const
{
	struct timeval timeout;
	timeout.tv_sec = 0;
	timeout.tv_usec = 0;

	fd_set poll_read_set;
	FD_ZERO(&poll_read_set);
	FD_SET(_handle, &poll_read_set);

	pollfd poll_thing;
	poll_thing.fd = _handle;
	poll_thing.events = poll_flag;
	poll_thing.revents = 0;

	#if defined(WIN32) | defined(_WIN64)
	int num_polled = WSAPoll(&poll_thing, 1, 0);
	#elif defined(__linux__)
	int num_polled = poll(&poll_thing, 1, 0);
	#endif
	if (num_polled > 0)
		return poll_thing.revents | poll_flag;
	
	throw_if_socket_error(num_polled, std::string("Failed to poll linux socket readability"));
	return false;
}

bool epic_socket::select_for(::select_for epic_for) const
{
	struct timeval timeout;
	timeout.tv_sec = 0;
	timeout.tv_usec = 10000;

	fd_set set;
	FD_ZERO(&set);
	FD_SET(_handle, &set);
	int n;
	switch (epic_for)
	{
	default:
	case select_for::READ:
		n = select(_handle + 1, &set, NULL, NULL, &timeout);
		break;
	case select_for::WRITE:
		n = select(_handle + 1, NULL, &set, NULL, &timeout);
		break;
	case select_for::EXCEPT:
		n = select(_handle + 1, NULL, NULL, &set, &timeout);
		break;
	}
	throw_if_socket_error(n, "Failed to select");
	return FD_ISSET(_handle, &set);
}

bool epic_socket::has_message() const
{
	struct timeval timeout;
	timeout.tv_sec = 0;
	timeout.tv_usec = 0;

	fd_set poll_read_set;
	FD_ZERO(&poll_read_set);
	FD_SET(_handle, &poll_read_set);

	int n = select(_handle + 1, &poll_read_set, 0, 0, &timeout);
	throw_if_socket_error(n, "Failed to poll linux socket readability");

	return n > 0;
}

bool epic_socket::has_died() const
{
	if (is_tcp())
	{
		if (has_message())
		{
			std::vector<char> recv_data{ 100, '0', std::allocator<char>() };
			int n = recv(_handle, recv_data.data(), recv_data.size(), MSG_PEEK);
			if (n == SOCKET_ERROR)
			{
				std::cerr << "[Data] Failed to peek data from linux socket (trying to determine if closed): " << get_last_error() << std::endl;
				return true;
			}
			return n == 0;
		}
	}
	if (is_udp())
		return false;
	throw_new_exception("Invalid protocol", LINE_CONTEXT);
}

raw_name_data epic_socket::get_peer_raw() const
{
	sockaddr_in peer_name;
	socklen_t peer_size = sizeof(peer_name);
	int n = getpeername(_handle, (sockaddr*)&peer_name, &peer_size);
	throw_if_socket_error(n, "Failed to getpeername");

	raw_name_data raw_data;
	raw_data.name = *(sockaddr*)&peer_name;
	raw_data.name_len = peer_size;
	return raw_data;
}

raw_name_data epic_socket::get_name_raw() const
{
	sockaddr_in peer_name;
	socklen_t peer_size = sizeof(peer_name);
	int n = getsockname(_handle, (sockaddr*)&peer_name, &peer_size);
	throw_if_socket_error(n, "Failed to getpeername");

	raw_name_data raw_data;
	raw_data.name = *(sockaddr*)&peer_name;
	raw_data.name_len = peer_size;
	return raw_data;
}

bool epic_socket::send_bytes(std::vector<char> bytes)
{
	if (is_tcp())
	{
		int result = send(_handle, bytes.data(), bytes.size(), 0);
		if (result == SOCKET_ERROR)
			return false;
		return true;
	}
	else if (is_udp())
	{
		int result = sendto(_handle, bytes.data(), bytes.size(), 0, &_endpoint.name, _endpoint.name_len);
		if (result == SOCKET_ERROR)
			return false;
		return true;
	}
	else
	{
		throw std::runtime_error("Can not send data with an invalid protocol");
	}
}

std::vector<char> epic_socket::recv_bytes()
{
	if (is_tcp())
	{
		std::vector<char> data(500, (char)0, std::allocator<char>());
		int result = ::recv(_handle, data.data(), (int)data.size(), 0);
		if (result == SOCKET_ERROR)
		{
			std::cout << "Receiving data failed" << std::endl;
			return std::vector<char>();
		}
		data.resize(result);
		return data;
	}
	if (is_udp())
	{
		while (true)
		{
			sockaddr addr;
			socklen_t addr_len;
			std::vector<char> data(500, (char)0, std::allocator<char>());
			int result = ::recvfrom(_handle, data.data(), (int)data.size(), 0, &addr, &addr_len);
			raw_name_data incoming{ addr, addr_len };
			if (incoming != _endpoint)
			{
				auto readable = incoming.as_readable();
				std::cout << "Receiving UDP data from an undesired endpoint (" << readable << ")" << std::endl;
				continue;
			}
			if (result == SOCKET_ERROR)
			{
				std::cerr << "Receiving (UDP) data failed: " << socket_error_to_string(result) << std::endl;
				return std::vector<char>();
			}
		}
	}
	throw_new_exception("Invalid protocol", LINE_CONTEXT);
}
