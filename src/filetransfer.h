#pragma once

#include "message.h"
#include "protocol.h"
#include "interfaces.h"

#include <string>
#include <vector>
#include <memory>
#include <fstream>

/// Messages:
/// MESSAGE_TYPE::PEER_FILE:
/// Contains the FileHeader
/// 
/// MESSAGE_TYPE::STREAM_CHUNK:
/// Contains a StreamChunk
/// 
/// MESSAGE_TYPE::CHUNK_ERROR:
/// Contains an int describing which chunk is missing
/// 
/// MESSAGE_TYPE::PEER_FILE_END:
/// Contains nothing, sent once a file has been fully sent

// Number of bytes stored in a StreamChunk's data vector
const int CHUNK_SIZE = 1000;

struct FileHeader {
    std::string filename;
    std::string extension;
    int num_chunks;
    int file_id;
};

struct StreamChunk {
    int file_id;
    int chunk_id;
    int data_length;
    std::vector<char> data;

    bool operator==(const StreamChunk& other) const;
    inline bool operator!=(const StreamChunk& other) const { return !(this->operator==(other)); }

    static const StreamChunk empty;
};

class FileSender {
    friend class FileTransfer;
    public:
        void sendFile(std::unique_ptr<IDataSocketWrapper>& socket);
        StreamChunk GetTargetChunk(int index);
        void onChunkError(const Message& mess, std::unique_ptr<IDataSocketWrapper>& socket);

    private:
        FileSender(const FileHeader& header, std::unique_ptr<IDataSocketWrapper>& socket);
        
        FileHeader _header;
        std::ifstream _file;
        std::vector<StreamChunk> _chunks;

        int _next_chunk_send = 0;
        void processFileToChunks(std::ifstream& ifs, std::vector<StreamChunk>& chunks);
        const StreamChunk& IterateNextChunk();
};

class FileReceiver {
    friend class FileTransfer;
    public:
        void onChunk(const Message& message, std::unique_ptr<IDataSocketWrapper>& socket);
        void onFileEnd(const Message& message);

    private:
        FileReceiver(const Message& message);

        FileHeader _header;
        std::vector<StreamChunk> _chunks;

};

class FileTransfer {
public:
    static std::unique_ptr<FileSender> BeginTransfer(const FileHeader& header, std::unique_ptr<IDataSocketWrapper>& socket);
    static std::unique_ptr<FileReceiver> BeginReception(const Message& message);
};

template<>
struct to_message<StreamChunk>
{
    Message operator()(const StreamChunk& val)
    {
        std::vector<char> data{};
        copy_to_message_struct::copy_to_message(data, val.chunk_id, val.data_length, val.data);
        return create_streammessage(MESSAGE_TYPE::STREAM_CHUNK, data);
    }
};

template<>
struct to_message<FileHeader>
{
    Message operator()(const FileHeader& val)
    {
        return create_message(MESSAGE_TYPE::PEER_FILE, val.filename, val.extension, val.num_chunks);
    }
};

template<>
struct from_message<StreamChunk>
{
    StreamChunk operator()(const Message& mess)
    {
        int read_index = 0;
        StreamChunk chunk;
        chunk.chunk_id = mess.read_type<decltype(chunk.chunk_id)>(read_index);
        chunk.data_length = mess.read_type<decltype(chunk.data_length)>(read_index);
        chunk.data = std::vector<char>(mess.Data.begin() + read_index, mess.Data.begin() + read_index + chunk.data_length);
        read_index += chunk.data_length;
        return chunk;
    }
};

template<>
struct from_message_with_crc<StreamChunk>
{
    StreamChunk operator()(const Message& mess, uint32_t& crc)
    {
        int read_index = 0;
        StreamChunk chunk;
        chunk.chunk_id = mess.read_type<decltype(chunk.chunk_id)>(read_index);
        chunk.data_length = mess.read_type<decltype(chunk.data_length)>(read_index);
        chunk.data = std::vector<char>(mess.Data.begin() + read_index, mess.Data.begin() + read_index + chunk.data_length);
        read_index += chunk.data_length;
        crc = mess.read_type<uint32_t>(read_index);
        return chunk;
    }
};

template<>
struct from_message<FileHeader>
{
    FileHeader operator()(const Message& mess)
    {
        int read_index = 0;
        FileHeader header;
        header.extension = mess.read_type<std::string>(read_index);
        header.filename = mess.read_type<std::string>(read_index);
        header.num_chunks = mess.read_type<decltype(header.num_chunks)>(read_index);
        return header;
    }
};