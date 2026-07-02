// =============================================================================
// crc_app.cpp — CRC Strategy + FrameCodec
// Used for IEC 60601-1-8 alarm communication frame encoding
// =============================================================================
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include "frame_codec.hpp"

LOG_MODULE_REGISTER(crc_app, LOG_LEVEL_INF);

static Crc16Ccitt g_crc16;
static FrameCodec g_codec(g_crc16);

void crc_app_init() {
    LOG_INF("CRC FrameCodec ready: CRC16-CCITT poly=0x1021");

    // Self-test: encode then decode a known frame
    Frame f;
    f.cmd = 0x01;
    f.payload_len = 5;
    __builtin_memcpy(f.payload, "ALARM", 5);

    uint8_t buf[FrameCodec::MAX_FRAME_BYTES];
    size_t n = g_codec.encode(f, buf, sizeof(buf));
    auto decoded = g_codec.decode(buf, n);

    if (decoded.has_value()) {
        LOG_INF("CRC self-test PASSED");
    } else {
        LOG_ERR("CRC self-test FAILED — check frame_codec.hpp");
    }
}

FrameCodec& crc_get_codec() { return g_codec; }
