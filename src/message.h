#pragma once

#include <vector>
#include <memory>
#include <string>
#include <cstring>
#include <stdexcept>



enum class ConnectionStatus
{
	PENDING = 0,
	SUCCESS = 1,
	FAILED = 2,
};

// Example Message Data: 
// MESSAGE_TYPE | MESSAGE_LENGTH | MESSAGE_DATA
// A Message will always be sizeof(MESSAGE_TYPE) + sizeof(MESSAGE_LENGTH) + MESSAGE_LENGTH bytes long

enum class MESSAGE_TYPE
{
	NONE = 0,
	MY_DATA,
	READY_FOR_P2P,
	CONNECT_TO_PEER,
	PEER_MSG,
	PEER_FILE,
	UDP_SYN,
	UDP_SYN_ACK,
	UDP_ACK,
	STREAM_CHUNK
	MISSING_CHUNK,
	PEER_FILE_END
};

typedef uint32_t MESSAGE_LENGTH_T;

struct Message
{
	MESSAGE_TYPE Type;
	MESSAGE_LENGTH_T Length;
	std::vector<char> Data;

	std::vector<char> to_bytes() const;

	inline bool operator==(const Message& other) const { return Type == other.Type && Length == other.Length && Data == other.Data; }

	template<class T>
	T read_type(int& read_index) const
	{
		int size = sizeof(T);
		if (read_index + size > Data.size())
			throw std::runtime_error("Not enough data to read");

		T* ptr = (T*)&(Data[read_index]);
		read_index += size;
		return *ptr;
	}

	template<>
	std::string read_type<std::string>(int& read_index) const
	{
		int size = sizeof(size_t);
		if (read_index + size > Data.size())
			throw std::runtime_error("Not enough data to read string length");

		size_t len = read_type<size_t>(read_index);
		if (read_index + len * sizeof(char) > Data.size())
			throw std::runtime_error("Not enough data to read string characters");

		read_index += (int)len;
		return std::string(Data.data() + read_index - len, Data.data() + read_index);
	}

	static const Message null_message;
};

inline bool message_is_type(const MESSAGE_TYPE& type, const Message& m) { return m.Type == type; }
std::vector<Message> data_to_messages(const std::vector<char>& data);

template<class T>
struct to_message;

template<>
struct to_message<MESSAGE_TYPE>
{
	Message operator()(const MESSAGE_TYPE& t)
	{
		Message mess;
		mess.Data = std::vector<char>();
		mess.Length = 0;
		mess.Type = t;
		return mess;
	}
};

template<class T>
struct from_message;

inline std::string mt_to_string(const MESSAGE_TYPE& t)
{
	switch (t)
	{
		case MESSAGE_TYPE::MY_DATA:
			return "MY_DATA: Connection's Private :3 Data";

		case MESSAGE_TYPE::READY_FOR_P2P:
			return "READY_FOR_P2P: Connection is ready for P2P";

		case MESSAGE_TYPE::PEER_MSG:
			return "MSG: Plain Text Msg";

		case MESSAGE_TYPE::PEER_FILE:
			return "FILE: Incoming File";

		case MESSAGE_TYPE::CONNECT_TO_PEER:
			return "CONNECT_PEER: Data required to connect to a peer";

		case MESSAGE_TYPE::NONE:
		default:
			return "NONE: None";
	}
}

template<class T, typename = std::enable_if_t<std::is_pod<T>::value>>
Message create_message(MESSAGE_TYPE type, T other_data)
{
	MESSAGE_LENGTH_T length = sizeof(other_data);
	Message mess;
	mess.Type = type;
	mess.Length = sizeof(other_data);
	mess.Data = std::vector<char>((char*)&other_data, ((char*)&other_data) + sizeof(other_data));
	return mess;
}

inline Message create_message(MESSAGE_TYPE type)
{
	Message data;
	data.Type = type;
	data.Length = 0;
	data.Data = std::vector<char>();
	return data;
}

inline Message create_message(MESSAGE_TYPE type, std::vector<char> data)
{
	Message mess;
	mess.Type = type;
	mess.Length = (MESSAGE_LENGTH_T)data.size();
	mess.Data = std::move(data);
	return mess;
}

struct copy_to_message_struct
{
	template<class... Types>
	static void copy_to_message(std::vector<char>& dst, Types... args);

	static void copy_to_message(std::vector<char>& dst);
};

template<class T, class... Types>
struct copy_to_message_template
{
	template<typename = std::enable_if_t<std::is_pod<T>::value>>
	constexpr static void copy(std::vector<char>& dst, const T& arg, Types... other_args)
	{
		static_assert(std::is_pod<T>::value || std::is_same<std::vector<char>, T>::value, "Can only use POD or std::vector<char> in create_message");
		dst.resize(dst.size() + sizeof(T));
		T* back = ((T*)(&dst.back() - sizeof(T) + 1));
		*back = arg;
		copy_to_message_struct::copy_to_message(dst, other_args...);
	}
};

template<>
struct copy_to_message_template<std::vector<char>>
{
	static void copy(std::vector<char>& dst, const std::vector<char>& src)
	{
		dst.insert(dst.end(), src.begin(), src.end());
	}
};

template<class... Types>
struct copy_to_message_template<std::vector<char>, Types...>
{
	static void copy(std::vector<char>& dst, const std::vector<char>& src, Types... other_args)
	{
		dst.insert(dst.end(), src.begin(), src.end());
		copy_to_message_struct::copy_to_message(dst, other_args...);
	}
};

template<class... Types>
struct copy_to_message_template<std::string, Types...>
{
	static void copy(std::vector<char>& dst, const std::string& src, Types... other_args)
	{
		size_t cur_size = dst.size();
		size_t str_len = src.length();
		dst.resize(cur_size + sizeof(size_t));
		std::memcpy(&dst[cur_size], &str_len, sizeof(str_len));
		dst.reserve(dst.size() + str_len);
		dst.insert(dst.end(), src.begin(), src.end());
		copy_to_message_struct::copy_to_message(dst, other_args...);
	}
};

template<class ...Types>
inline void copy_to_message_struct::copy_to_message(std::vector<char>& dst, Types ...args)
{
	copy_to_message_template<Types...>::copy(dst, args...);
}

inline void copy_to_message_struct::copy_to_message(std::vector<char>& dst) {}

template<typename... Types>
inline Message create_message(MESSAGE_TYPE type, Types... args)
{
	Message mess;
	mess.Type = type;
	mess.Data = std::vector<char>{};
	copy_to_message_struct::copy_to_message(mess.Data, args...);
	mess.Length = (MESSAGE_LENGTH_T)mess.Data.size();
	return mess;
}

inline Message create_message(MESSAGE_TYPE type, std::string data)
{
	Message mess;
	mess.Type = type;
	size_t len = data.length();
	mess.Data = std::vector<char>(sizeof(len), '0');
	std::memcpy(mess.Data.data(), &len, sizeof(len));
	mess.Data.reserve(sizeof(type) + sizeof(len) + data.length());
	mess.Data.insert(mess.Data.end(), data.begin(), data.end());
	mess.Length = (MESSAGE_LENGTH_T)mess.Data.size();
	return mess;
}
