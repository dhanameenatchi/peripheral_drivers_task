// =============================================================================
// frame_codec.cpp — FrameCodec strategy encoder and decoder implementation
// =============================================================================
#include "frame_codec.hpp"
#include <cstring>

FrameCodec::FrameCodec(const ICrcStrategy& crc) : crc_(crc) {}

size_t FrameCodec::encode(const Frame& f, uint8_t* out, size_t out_size) const {
    if (!out) return 0;
    size_t total = 3 + f.payload_len + crc_.crcSize();
    if (total > out_size) return 0;

    out[0] = Frame::SOF;
    out[1] = static_cast<uint8_t>(f.payload_len);
    out[2] = f.cmd;
    std::memcpy(out + 3, f.payload, f.payload_len);

    uint16_t crc = crc_.compute({out, 3 + f.payload_len});
    if (crc_.crcSize() == 2) {
        out[3 + f.payload_len]     = static_cast<uint8_t>(crc >> 8);
        out[3 + f.payload_len + 1] = static_cast<uint8_t>(crc & 0xFF);
    } else {
        out[3 + f.payload_len] = static_cast<uint8_t>(crc & 0xFF);
    }
    return total;
}

std::optional<Frame> FrameCodec::decode(const uint8_t* in, size_t len) const {
    if (!in || len < 3 + crc_.crcSize()) return std::nullopt;
    if (in[0] != Frame::SOF) return std::nullopt;

    uint8_t payload_len = in[1];
    if (payload_len > Frame::MAX_PAYLOAD) return std::nullopt;

    size_t expected = 3 + payload_len + crc_.crcSize();
    if (len < expected) return std::nullopt;

    // Compute CRC over header + payload
    uint16_t computed = crc_.compute({in, 3 + payload_len});
    uint16_t received;
    if (crc_.crcSize() == 2) {
        received = (static_cast<uint16_t>(in[3 + payload_len]) << 8) |
                    in[3 + payload_len + 1];
    } else {
        received = in[3 + payload_len];
    }

    if (computed != received) return std::nullopt;

    Frame f;
    f.cmd         = in[2];
    f.payload_len = payload_len;
    std::memcpy(f.payload, in + 3, payload_len);
    return f;
}
