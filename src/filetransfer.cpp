#include "filetransfer.h"

#include <cstdint>
#include <array>

StreamChunk FileSender::IterateNextChunk()
{
	return StreamChunk();
}

StreamChunk FileSender::GetTargetChunk(int index)
{
	return StreamChunk();
}

StreamChunk FileSender::onMissingChunk(const Message& mess)
{
	int read_index = 0;
	int missing_id = mess.read_type<int>(read_index);

	if (missing_id < 0 || missing_id >= _header.num_chunks)
	{
		std::cerr << "Received MISSING_CHUNK message with invalid chunk id" << std::endl;
		
	}
	return StreamChunk{};// ....for now
}

void FileSender::beginSending(PtopSocket& socket)
{
	processFileToChunks(_file, _chunks);
	_header.num_chunks = _chunks.size();
	auto mess = to_message<FileHeader>()(_header);
	socket.send_bytes(mess.to_bytes());
}

void FileSender::processFileToChunks(std::ifstream& ifs, std::vector<StreamChunk>& chunks)
{
	std::streamsize last_read_count = 0;
#ifdef max
#define dumb_tmp max
#undef max
#endif
	auto max_size = std::numeric_limits<std::streamsize>::max();
#ifdef dumb_tmp
#define max dumb_tmp
#undef dumb_tmp
#endif
	int running_id = 0;
	while (last_read_count != max_size)
	{
		StreamChunk chunk;
		chunk.chunk_id = running_id++;
		chunk.data = std::vector<char>(CHUNK_SIZE, (char)0);
		ifs.read(chunk.data.data(), CHUNK_SIZE);

		last_read_count = ifs.gcount();
		chunk.data_length = last_read_count;
		chunk.data.resize(last_read_count);

		chunk.chunk_hash = hash_data(chunk.data);

		chunks.push_back(chunk);
	}
}

void FileReceiver::onChunk(const Message& message)
{
}

void FileReceiver::onFileEnd(const Message& message)
{
}

FileReceiver::FileReceiver(const Message& message)
{
}

void FileReceiver::checksum()
{
}

void FileReceiver::order_chunk()
{
}

std::unique_ptr<FileSender> FileTransfer::BeginTransfer(FileHeader header, PtopSocket socket)
{
	return std::unique_ptr<FileSender>();
}

std::unique_ptr<FileReceiver> FileTransfer::BeginReception(const Message& message)
{
	return std::unique_ptr<FileReceiver>();
}

constexpr uint32_t crc_polynomial = 0x34135u;

constexpr auto crc_table = [] {
	auto width = sizeof(uint32_t) * 8;
	auto topbit = 1u << ((uint32_t)(width - 1));
    std::array<uint32_t, 256> tbl{};
    for (int dividend = 0; dividend < 256; ++dividend) {
        uint32_t remainder = dividend << (width - 8);
        for (uint8_t bit = 8; bit > 0; --bit) {
            if (remainder & topbit) {
                remainder = (remainder << 1) ^ crc_polynomial;
            } else {
                remainder = (remainder << 1);
            }
        }
        tbl[dividend] = remainder;
    }
    return tbl;
}();

uint64_t hash_data(const std::vector<char>& data)
{
	// well.... crap
	return 0;
}
