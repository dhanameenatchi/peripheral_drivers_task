#pragma once
// =============================================================================
// CRC — Strategy Pattern  +  Binary Frame Protocol
// ICrcStrategy: Crc16Ccitt (0x1021) / Crc8Maxim (0x31)
// FrameCodec:  [SOF 0xAA | Len 1B | Cmd 1B | Payload ≤32B | CRC]
// Medical context: IEC 60601-1-8 alarm frame integrity
// =============================================================================
#include "crc_strategy.hpp"
#include "crc16_ccitt.hpp"
#include "crc8_maxim.hpp"
#include <cstdint>
#include <optional>

// ---------------------------------------------------------------------------
// Frame layout: [0xAA | Len | Cmd | Payload[0..32] | CRC(1 or 2 bytes)]
// ---------------------------------------------------------------------------
struct Frame {
    static constexpr uint8_t SOF         = 0xAA;
    static constexpr size_t  MAX_PAYLOAD = 32;

    uint8_t  cmd     = 0;
    uint8_t  payload[MAX_PAYLOAD]{};
    uint8_t  payload_len = 0;
};

// ---------------------------------------------------------------------------
// FrameCodec — encode / decode with pluggable CRC strategy
// ---------------------------------------------------------------------------
class FrameCodec {
public:
    static constexpr size_t MAX_FRAME_BYTES = 1 + 1 + 1 + Frame::MAX_PAYLOAD + 2;

    explicit FrameCodec(const ICrcStrategy& crc);

    // Encode frame into out[]. Returns number of bytes written (0 on error).
    size_t encode(const Frame& f, uint8_t* out, size_t out_size) const;

    // Decode bytes into Frame. Returns nullopt on bad SOF, length, or CRC.
    std::optional<Frame> decode(const uint8_t* in, size_t len) const;

private:
    const ICrcStrategy& crc_;
};
