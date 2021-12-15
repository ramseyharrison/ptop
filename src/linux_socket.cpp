#include "linux_socket.h"

#include <exception>
#include <stdexcept>
#include <iostream>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h> 
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <netdb.h>

std::string linux_error()
{
	auto err_code = errno;
	std::string error = "Err code: " + std::to_string(err_code) + " (";
	error += strerror(errno);
	return error + ")";
}

readable_ip_info convert_to_readable(raw_name_data data)
{
	std::vector<char> buf{ 50, '0', std::allocator<char>() };
	const char* str = inet_ntop(AF_INET, &data.addr, buf.data(), buf.size());
	if (!str)
		throw std::runtime_error(std::string("Failed to convert sockaddr to string: ") + linux_error());

	std::string address = str;

	std::string port = std::to_string(ntohs(data.addr.sin_port));
	readable_ip_info out;
	out.ip_address = address;
	out.port = port;
	return out;
}

void ILinuxSocket::update_name_info()
{
	auto name = get_myname_readable();
	_address = name.ip_address;
	_port = name.port;
}

void ILinuxSocket::update_endpoint_info()
{
	auto name = get_peername_readable();
	_endpoint_address = name.ip_address;
	_endpoint_port = name.port;
}

void ILinuxSocket::shutdown()
{
}

readable_ip_info ILinuxSocket::get_peer_data()
{
	sockaddr_in peer_name;
	socklen_t peer_size = sizeof(peer_name);
	int n = getpeername(_socket, (sockaddr*)&peer_name, &peer_size);

	std::vector<char> buf{ 50, '0', std::allocator<char>() };
	const char* str = inet_ntop(AF_INET, &peer_name.sin_addr, buf.data(), buf.size());
	if (!str)
		throw std::runtime_error(std::string("Failed to convert sockaddr to string: ") + linux_error());

	std::string address = str;

	std::string port = std::to_string(peer_name.sin_port);
	readable_ip_info out;
	out.ip_address = address;
	out.port = port;
	return out;
}

raw_name_data ILinuxSocket::get_sock_data()
{
	sockaddr_in peer_name;
	socklen_t peer_size = sizeof(peer_name);
	int n = getsockname(_socket, (sockaddr*)&peer_name, &peer_size);

	raw_name_data out;
	out.addr = peer_name;
	return out;
}

raw_name_data ILinuxSocket::get_peername_raw()
{
	sockaddr_in peer_name;
	socklen_t peer_size = sizeof(peer_name);
	int n = getpeername(_socket, (sockaddr*)&peer_name, &peer_size);
	if (n < 0)
		throw std::runtime_error(std::string("[Socket] Failed to getpeername with: ") + linux_error());

	raw_name_data raw_data;
	raw_data.addr = peer_name;
	return raw_data;
}

raw_name_data ILinuxSocket::get_myname_raw()
{
	sockaddr_in peer_name;
	socklen_t peer_size = sizeof(peer_name);
	int n = getsockname(_socket, (sockaddr*)&peer_name, &peer_size);
	if (n < 0)
		throw std::runtime_error(std::string("[Socket] Failed to getpeername with: ") + linux_error());

	raw_name_data raw_data;
	raw_data.addr = peer_name;
	return raw_data;
}

readable_ip_info ILinuxSocket::get_peername_readable()
{
	return convert_to_readable(get_peername_raw());
}

readable_ip_info ILinuxSocket::get_myname_readable()
{
	return convert_to_readable(get_myname_raw());
}

std::string ILinuxSocket::get_my_ip()
{
	if (_address == "Unassigned")
		update_name_info();
	return _address;
}

std::string ILinuxSocket::get_my_port()
{
	if (_port == "Unassigned")
		update_name_info();
	return _port;
}

std::string ILinuxSocket::get_endpoint_ip()
{
	if (_endpoint_address == "Unassigned")
		update_endpoint_info();
	return _endpoint_address;
}

std::string ILinuxSocket::get_endpoint_port()
{
	if (_endpoint_port == "Unassigned")
		update_endpoint_info();
	return _endpoint_port;
}

linux_listen_socket::linux_listen_socket(std::string port)
{
	std::cout << "[Listen] Create new Socket on port (with localhost): " << port << std::endl;
	_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

	socklen_t cli_len;
	if (_socket < 0)
		throw std::runtime_error(std::string("[Listen] Failed to create linux socket: ") + linux_error());

	struct sockaddr_in serv_addr, cli_addr;

	int portno = atoi(port.c_str());
	bzero((char*)&serv_addr, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = INADDR_ANY;
	serv_addr.sin_port = htons(portno);

	std::cout << "Binding..." << std::endl;
	if (bind(_socket, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0)
		throw std::runtime_error(std::string("[Listen] Failed to bind linux socket: ") + linux_error());
	std::cout << "[Listen] Post Bind Check: Bound to: " << get_my_ip() << ":" << get_my_port() << std::endl;
}

void linux_listen_socket::listen()
{
	std::cout << "[Listen] Socket now Listening (" << get_my_ip() << ":" << get_my_port() << ")" << std::endl;
	if (::listen(_socket, 5) < 0)
		throw std::runtime_error(std::string("[Listen] Error when listening: ") + linux_error());
}

bool linux_listen_socket::has_connection()
{
	struct timeval timeout;
	timeout.tv_sec = 0;
	timeout.tv_usec = 0;

	fd_set poll_read_set;
	FD_ZERO(&poll_read_set);
	FD_SET(_socket, &poll_read_set);

	int n = select(1, &poll_read_set, 0, 0, &timeout);
	if (n < 0)
		throw std::runtime_error(std::string("[Listen] Failed to poll linux socket readability: ") + linux_error());

	return n > 0;
}

std::unique_ptr<IDataSocket> linux_listen_socket::accept_connection()
{
	std::cout << "[Listen] Socket Attempting to accept a connection" << std::endl;
	sockaddr_in client_addr;
	socklen_t client_len;
	int new_socket = accept(_socket, (struct sockaddr*)&client_addr, &client_len);
	if (new_socket < 0)
		return nullptr;

	raw_name_data name;
	name.addr = client_addr;
	auto readable = convert_to_readable(name);
	std::cout << "[Listen] Accepted a connection: " << readable.ip_address << ":" << readable.port << std::endl;
	return std::make_unique<linux_data_socket>(new_socket, name);
}

linux_data_socket::linux_data_socket(int socket, raw_name_data name) : ILinuxSocket(socket, name)
{
	auto readable = convert_to_readable(name);
	std::cout << "[Data] Copy Constructor Data socket with endpoint: " << readable.ip_address << ":" << readable.port << std::endl;
}

linux_data_socket::linux_data_socket(std::string peer_address, std::string peer_port)
{
	std::cout << "[Data] Creating a Linux Data Socket connecting to: " << peer_address << ":" << peer_port << std::endl;
	struct sockaddr_in serv_addr;
	struct hostent* serv_ent;
	int portno = atoi(peer_port.c_str());

	serv_ent = gethostbyname(peer_address.c_str());

	memset(&serv_addr, 0, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	memcpy(&serv_addr.sin_addr.s_addr, &serv_ent->h_addr, serv_ent->h_length);
	serv_addr.sin_port = htons(portno);

	_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

	if (connect(_socket, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0)
		throw std::runtime_error(std::string("Socket failed to connect with: ") + linux_error());
}

std::vector<char> linux_data_socket::receive_data()
{
	std::cout << "[Data] Trying to received data from Socket: (" << get_my_ip() << ":" << get_my_port() << ", " << get_endpoint_ip() << ":" << get_endpoint_port() << ")" << std::endl;
	std::vector<char> recv_data{ 500, '0', std::allocator<char>() };
	int n = read(_socket, recv_data.data(), 500);
	if (n < -1)
	{
		std::cerr << "[Data] Failed to read data from linux socket: " << linux_error() << std::endl;
		return std::vector<char>();
	}
	recv_data.resize(n);
	return recv_data;
}

bool linux_data_socket::has_data()
{
	struct timeval timeout;
	timeout.tv_sec = 0;
	timeout.tv_usec = 0;

	fd_set poll_read_set;
	FD_ZERO(&poll_read_set);
	FD_SET(_socket, &poll_read_set);

	int n = select(1, &poll_read_set, 0, 0, &timeout);
	if (n < 0)
		throw std::runtime_error(std::string("[Data] Failed to poll linux socket readability: ") + linux_error());

	return n > 0;
}

bool linux_data_socket::send_data(const std::vector<char>& data)
{
	std::cout << "Sending " << data.size() << " bytes to: " << "(" << get_my_ip() << ":" << get_my_port() << ", " << get_endpoint_ip() << " : " << get_endpoint_port() << ") (priv, pub)" << std::endl;
	int n = write(_socket, data.data(), data.size());
	if (n < 0)
	{
		std::cerr << "Error sending data: " << linux_error() << std::endl;
		return false;
	}

	return true;
}

linux_reuse_nonblock_listen_socket::linux_reuse_nonblock_listen_socket(std::string port)
{
	std::cout << "[ListenReuseNoB] Creating Reusable Listen Socket on (localhost): " << port << std::endl;

	int portno = atoi(port.c_str());

	struct sockaddr_in serv_addr;
	memset(&serv_addr, 0, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = INADDR_ANY;
	serv_addr.sin_port = htons(portno);

	_socket = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, IPPROTO_TCP);
	if (_socket < 0)
		throw std::runtime_error(std::string("[ListenReuseNoB] Failed to create reusable nonblocking listen socket: ") + linux_error());

	int reuseVal = 1;
	int n = setsockopt(_socket, SOL_SOCKET, SO_REUSEADDR, &reuseVal, sizeof(reuseVal));
	if (n < 0)
	{
		auto err = linux_error();
		close(_socket);
		throw std::runtime_error(std::string("[ListenReuseNoB] Failed to set socket SO_REUSEADDR: ") + err);
	}

	n = ::bind(_socket, (struct sockaddr*)&serv_addr, sizeof(serv_addr));
	if (n < 0)
	{
		auto err = linux_error();
		close(_socket);
		throw std::runtime_error(std::string("[ListenReuseNoB] Failed to bind reuseable nonblocking socket: ") + err);
	}
}

void linux_reuse_nonblock_listen_socket::listen()
{
	std::cout << "[ListenReuseNoB] Now Listening on: " << get_my_ip() << ":" << get_my_port() << std::endl;
	auto n = ::listen(_socket, 4);
	if (n < 1 && n != EINPROGRESS)
		throw std::runtime_error(std::string("[ListenReuseNoB] Failed to listen with: ") + linux_error());
}

bool linux_reuse_nonblock_listen_socket::has_connection()
{
	struct timeval timeout;
	timeout.tv_sec = 0;
	timeout.tv_usec = 0;

	fd_set poll_read_set;
	FD_ZERO(&poll_read_set);
	FD_SET(_socket, &poll_read_set);

	int n = select(1, &poll_read_set, 0, 0, &timeout);
	if (n < 0)
		throw std::runtime_error(std::string("[ListenReuseNoB] Failed to poll linux socket readability (has connection): ") + linux_error());

	return n > 0;
}

std::unique_ptr<IDataSocket> linux_reuse_nonblock_listen_socket::accept_connection()
{
	std::cout << "[ListenReuseNoB] Accepting Connection..." << std::endl;
	sockaddr_in client_addr;
	socklen_t client_len;
	int accepted_socket = accept(_socket, (struct sockaddr*)&client_addr, &client_len);

	if (accepted_socket < 0)
		return nullptr;
	raw_name_data name;
	name.addr = client_addr;
	auto readable = convert_to_readable(name);
	std::cout << "[ListenReuseNoB] Accepted Connection from: " << readable.ip_address << ":" << readable.port << std::endl;
	return std::make_unique<linux_data_socket>(accepted_socket, name);
}

linux_reuse_nonblock_connection_socket::linux_reuse_nonblock_connection_socket(raw_name_data data)
{
	auto readable = convert_to_readable(data);
	std::cout << "[DataReuseNoB] Creating Connection socket to: " << readable.ip_address << ":" << readable.port << std::endl;
	_socket = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, IPPROTO_TCP);
	if (_socket < 0)
	{
		auto err = linux_error();
		close(_socket);
		throw std::runtime_error(std::string("[DataReuseNoB] Failed to create nonblocking socket: ") + err);
	}

	int reuseVal = 1;
	int n = setsockopt(_socket, SOL_SOCKET, SO_REUSEADDR, &reuseVal, sizeof(reuseVal));
	if (n < 0)
	{
		auto err = linux_error();
		close(_socket);
		throw std::runtime_error(std::string("[DataReuseNoB] Failed to set socket SO_REUSEADDR with: ") + err);
	}

	n = ::bind(_socket, (struct sockaddr*)&data.addr, sizeof(data.addr));
	if (n < 0)
	{
		auto err = linux_error();
		close(_socket);
		throw std::runtime_error(std::string("[DataReuseNoB] Failed to bind connect socket with: ") + err);
	}
	std::cout << "[DataReuseNoB] Successfully bound Data socket to: " << readable.ip_address << ":" << readable.port << std::endl;
}

void linux_reuse_nonblock_connection_socket::connect(std::string ip_address, std::string port)
{
	std::cout << "[DataReuseNoB] Tring to connect to: " << ip_address << ":" << port << std::endl;
	struct sockaddr_in serv_addr;
	struct hostent* serv_ent;
	int portno = atoi(port.c_str());

	serv_ent = gethostbyname(ip_address.c_str());

	memset(&serv_addr, 0, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	memcpy(&serv_addr.sin_addr.s_addr, &serv_ent->h_addr, serv_ent->h_length);
	serv_addr.sin_port = htons(portno);

	int n = ::connect(_socket, (struct sockaddr*)&serv_addr, sizeof(serv_addr));
	if (n < 0 && n != EINPROGRESS)
		throw std::runtime_error(std::string("Unexpected error attempting to connect on nonblocking socket: ") + linux_error());
	std::cout << "[DataReuseNoB] Successfully Connected to: " << ip_address << ":" << port << std::endl;
}

ConnectionStatus linux_reuse_nonblock_connection_socket::has_connected()
{
	fd_set write_set;
	FD_ZERO(&write_set);
	FD_SET(_socket, &write_set);

	struct timeval timeout;
	timeout.tv_sec = timeout.tv_usec = 0;

	int n = select(1, NULL, &write_set, NULL, &timeout);

	if (n < 0)
		throw std::runtime_error(std::string("Failed to select nonblock connect socket write-ability (whether it has connected): ") + linux_error());

	if (n < 1)
	{
		fd_set except_set;
		FD_ZERO(&except_set);
		FD_SET(_socket, &except_set);
		n = select(1, NULL, NULL, &except_set, &timeout);
		if (n < 0)
			throw std::runtime_error(std::string("Failed to select socket error status with: ") + linux_error());
		if (n < 1)
			return ConnectionStatus::PENDING;

		int sock_error = 0;
		socklen_t sock_error_size = sizeof(sock_error);
		if (getsockopt(_socket, SOL_SOCKET, SO_ERROR, (char*)&sock_error, &sock_error_size) < 0)
			throw std::runtime_error(std::string("Failed to get socket error code with: ") + linux_error());

		std::cerr << "Socket has error code: " << sock_error << std::endl;

		return ConnectionStatus::FAILED;
	}

	return ConnectionStatus::SUCCESS;
}

std::unique_ptr<IDataSocket> linux_reuse_nonblock_connection_socket::convert_to_datasocket()
{
	std::cout << "[DataReuseNoB] Converting to regular Data socket " << "(" << get_my_ip() << ":" << get_my_port() << ", " << get_endpoint_ip() << " : " << get_endpoint_port() << ") (priv, pub)" << std::endl;
	int flags = fcntl(_socket, F_GETFL);
	if (flags < 0)
		throw std::runtime_error(std::string("Failed to query socket's flags: ") + linux_error());
	int n = fcntl(_socket, F_SETFL, flags & (~O_NONBLOCK));
	if (n < 0)
		throw std::runtime_error(std::string("Failed to set socket as blocking again: ") + linux_error());

	auto raw_name = get_peername_raw();
	std::unique_ptr<IDataSocket> out = std::make_unique<linux_data_socket>(_socket, raw_name);
	_socket = -1;
	return out;
}