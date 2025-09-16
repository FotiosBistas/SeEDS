// snds_ipc_tcp.hpp
#pragma once

#include <array>
#include <cstdint>

#include <boost/asio.hpp>
#include <nlohmann/json.hpp>

namespace snds_ipc_tcp 
{

/**
 * @brief Write a 32-bit unsigned integer into a 4-byte buffer in big-endian order.
 *
 * @param v   The 32-bit unsigned integer value.
 * @param out A 4-byte char buffer to receive the encoded bytes.
 *
 * Example: value 0x12345678 → bytes {0x12, 0x34, 0x56, 0x78}.
 */
inline void write_be32(uint32_t v, char out[4]) 
{
    out[0] = static_cast<char>((v >> 24) & 0xFF);
    out[1] = static_cast<char>((v >> 16) & 0xFF);
    out[2] = static_cast<char>((v >> 8) & 0xFF);
    out[3] = static_cast<char>(v & 0xFF);
}

/**
 * @brief Read a 32-bit unsigned integer from a 4-byte buffer in big-endian order.
 *
 * @param in A 4-byte buffer containing the encoded integer.
 * @return   The decoded 32-bit unsigned integer value.
 *
 * Example: bytes {0x12, 0x34, 0x56, 0x78} → value 0x12345678.
 */
inline uint32_t read_be32(const char in[4]) 
{
    return (static_cast<uint8_t>(in[0]) << 24) | (static_cast<uint8_t>(in[1]) << 16) | (static_cast<uint8_t>(in[2]) << 8)  | static_cast<uint8_t>(in[3]);
}

/**
 * @brief Serialize JSON into a length-prefixed frame and send it over a Boost.Asio stream.
 *
 * This function first serializes the JSON object into a compact string using `dump()`,
 * then prefixes it with a 4-byte big-endian length header, and finally writes both
 * buffers to the provided synchronous stream (e.g. `boost::asio::ip::tcp::socket`).
 *
 * @tparam SyncWriteStream A synchronous write-capable stream (e.g. TCP socket).
 * @param s  The stream object to write to.
 * @param j  The JSON object to serialize and send.
 *
 * Usage example:
 *   boost::asio::ip::tcp::socket sock(io_context);
 *   snds_ipc_tcp::send_json(sock, nlohmann::json{{"kind","patch"},{"payload",{...}}});
 */
template<class SyncWriteStream>
inline void send_json(SyncWriteStream& s, const nlohmann::json& j) 
{
    const std::string body = j.dump();
    char hdr[4]; 
    write_be32(static_cast<uint32_t>(body.size()), hdr);
    std::array<boost::asio::const_buffer,2> bufs
    {
        boost::asio::buffer(hdr,4), 
        boost::asio::buffer(body)
    };
    boost::asio::write(s, bufs);
}

} // namespace snds_ipc_tcp
