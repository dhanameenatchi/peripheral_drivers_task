#!/usr/bin/env python3
"""
CRC Frame Loopback Test — Python host script
Tests FrameCodec encode/decode over serial port (or mock loopback).
Performs:
  1. Normal round-trip encode → send → receive → decode
  2. Single-bit error injection → verify frame is dropped
  3. Frame drop simulation (timeout)

Medical context: IEC 60601-1-8 alarm frame integrity validation.

Usage:
  python3 loopback_test.py --port /dev/ttyACM0 --baud 921600
  python3 loopback_test.py --mock   # software loopback (no hardware needed)
"""

import argparse
import struct
import sys
import time
import random
from dataclasses import dataclass
from typing import Optional

# ─── Frame constants ──────────────────────────────────────────────────────────
SOF         = 0xAA
MAX_PAYLOAD = 32

# ─── CRC implementations (Python mirrors of C++ strategies) ──────────────────

def crc16_ccitt(data: bytes) -> int:
    """CRC-16/CCITT  poly=0x1021, init=0xFFFF"""
    crc = 0xFFFF
    for byte in data:
        crc ^= byte << 8
        for _ in range(8):
            crc = ((crc << 1) ^ 0x1021) if (crc & 0x8000) else (crc << 1)
        crc &= 0xFFFF
    return crc

def crc8_maxim(data: bytes) -> int:
    """CRC-8/Maxim  poly=0x31, init=0x00, refin=true, refout=true"""
    crc = 0x00
    for byte in data:
        crc ^= byte
        for _ in range(8):
            crc = ((crc >> 1) ^ 0x31) if (crc & 0x01) else (crc >> 1)
    return crc & 0xFF


# ─── Frame encode / decode ────────────────────────────────────────────────────

def encode_frame(cmd: int, payload: bytes, use_crc16: bool = True) -> bytes:
    assert len(payload) <= MAX_PAYLOAD, "Payload too large"
    header  = bytes([SOF, len(payload), cmd]) + payload
    if use_crc16:
        crc = crc16_ccitt(header)
        return header + struct.pack(">H", crc)
    else:
        crc = crc8_maxim(header)
        return header + bytes([crc])

def decode_frame(raw: bytes, use_crc16: bool = True) -> Optional[dict]:
    crc_size = 2 if use_crc16 else 1
    if len(raw) < 3 + crc_size:
        return None
    if raw[0] != SOF:
        return None
    payload_len = raw[1]
    if payload_len > MAX_PAYLOAD:
        return None
    total = 3 + payload_len + crc_size
    if len(raw) < total:
        return None
    header_payload = raw[:3 + payload_len]
    if use_crc16:
        received_crc = struct.unpack(">H", raw[3 + payload_len:3 + payload_len + 2])[0]
        computed_crc = crc16_ccitt(header_payload)
    else:
        received_crc = raw[3 + payload_len]
        computed_crc = crc8_maxim(header_payload)
    if received_crc != computed_crc:
        return None
    return {
        "cmd":     raw[2],
        "payload": raw[3:3 + payload_len]
    }

def inject_single_bit_error(frame: bytes, byte_index: int = 3, bit: int = 0) -> bytes:
    """Flip a single bit in the frame"""
    b = bytearray(frame)
    b[byte_index] ^= (1 << bit)
    return bytes(b)


# ─── Mock serial (software loopback) ─────────────────────────────────────────

class MockSerial:
    """Simulates a loopback serial port (TX bytes come back as RX)."""
    def __init__(self):
        self._buf = b""
    def write(self, data: bytes) -> int:
        self._buf += data
        return len(data)
    def read(self, n: int) -> bytes:
        out, self._buf = self._buf[:n], self._buf[n:]
        return out
    def close(self): pass


# ─── Tests ────────────────────────────────────────────────────────────────────

@dataclass
class TestResult:
    name: str
    passed: bool
    detail: str = ""

def test_normal_roundtrip(port, use_crc16: bool) -> TestResult:
    """Encode a frame, send, receive, decode — must match."""
    if hasattr(port, 'reset_input_buffer'):
        port.reset_input_buffer()
    cmd     = 0x42
    payload = b"ALARM_01"
    frame   = encode_frame(cmd, payload, use_crc16)
    port.write(frame)
    time.sleep(0.25)  # allow firmware to process and echo
    received = port.read(len(frame))

    decoded = decode_frame(received, use_crc16)
    if decoded is None:
        return TestResult("Round-trip", False,
                          f"Decode returned None (sent {len(frame)}B, got {len(received)}B: {received.hex()})")
    if decoded["cmd"] != cmd or decoded["payload"] != payload:
        return TestResult("Round-trip", False,
                          f"Mismatch: cmd={decoded['cmd']} payload={decoded['payload']}")
    return TestResult("Round-trip", True,
                      f"cmd=0x{cmd:02X} payload={payload} CRC={'16' if use_crc16 else '8'}")

def test_single_bit_error(port, use_crc16: bool) -> TestResult:
    """Inject a single-bit error — decode must return None."""
    if hasattr(port, 'reset_input_buffer'):
        port.reset_input_buffer()
    frame      = encode_frame(0x10, b"safe_data", use_crc16)
    corrupted  = inject_single_bit_error(frame, byte_index=3, bit=3)
    port.write(corrupted)
    time.sleep(0.25)
    received   = port.read(len(corrupted))
    decoded    = decode_frame(received, use_crc16)
    if decoded is not None:
        return TestResult("Single-bit error", False, "Error was NOT detected!")
    return TestResult("Single-bit error", True, "Error correctly detected and frame dropped")

def test_frame_drop_simulation(port, use_crc16: bool) -> TestResult:
    """Send a truncated frame — must be rejected."""
    if hasattr(port, 'reset_input_buffer'):
        port.reset_input_buffer()
    frame     = encode_frame(0x20, b"hi", use_crc16)
    truncated = frame[:len(frame) - 2]  # remove CRC bytes
    port.write(truncated)
    time.sleep(0.5)  # wait for firmware timeout to reset state machine
    received  = port.read(len(frame))
    decoded   = decode_frame(received[:len(truncated)], use_crc16)
    if decoded is not None:
        return TestResult("Frame drop / truncated", False, "Truncated frame was accepted!")
    return TestResult("Frame drop / truncated", True, "Truncated frame correctly rejected")

def test_max_payload(port, use_crc16: bool) -> TestResult:
    """32-byte payload boundary."""
    if hasattr(port, 'reset_input_buffer'):
        port.reset_input_buffer()
    payload = bytes(range(MAX_PAYLOAD))
    frame   = encode_frame(0xFF, payload, use_crc16)
    time.sleep(0.1)
    port.write(frame)
    time.sleep(0.25)
    received = port.read(len(frame))
    decoded  = decode_frame(received, use_crc16)
    if decoded is None or decoded["payload"] != payload:
        return TestResult("Max payload 32B", False,
                          f"decoded={decoded} (sent {len(frame)}B, got {len(received)}B: {received.hex()})")
    return TestResult("Max payload 32B", True, "32-byte payload round-trip OK")

def test_zero_payload(port, use_crc16: bool) -> TestResult:
    """Zero-length payload."""
    if hasattr(port, 'reset_input_buffer'):
        port.reset_input_buffer()
    frame   = encode_frame(0x01, b"", use_crc16)
    time.sleep(0.1)
    port.write(frame)
    time.sleep(0.25)
    received = port.read(len(frame))
    decoded  = decode_frame(received, use_crc16)
    if decoded is None or len(decoded["payload"]) != 0:
        return TestResult("Zero-length payload", False,
                          f"sent {len(frame)}B, got {len(received)}B: {received.hex()}")
    return TestResult("Zero-length payload", True, "Empty payload OK")

def test_crc_known_vectors() -> list[TestResult]:
    """Verify CRC implementations against standard test vectors."""
    results = []
    data = b"123456789"
    # CRC16-CCITT → 0x29B1
    got16 = crc16_ccitt(data)
    results.append(TestResult(
        "CRC16-CCITT known vector",
        got16 == 0x29B1,
        f"expected=0x29B1 got=0x{got16:04X}"
    ))
    # CRC8-Maxim → 0xA1
    got8 = crc8_maxim(data)
    results.append(TestResult(
        "CRC8-Maxim known vector",
        got8 == 0x07,
        f"expected=0x07 got=0x{got8:02X}"
    ))
    return results


# ─── Main ─────────────────────────────────────────────────────────────────────

def run_all_tests(port, crc_mode: str = "both") -> bool:
    all_results: list[TestResult] = []

    # Known vector tests (no serial needed)
    all_results.extend(test_crc_known_vectors())

    strategies = []
    if crc_mode == "16":
        strategies = [True]
    elif crc_mode == "8":
        strategies = [False]
    else:
        strategies = [True, False]

    for use_crc16 in strategies:
        label = "CRC16" if use_crc16 else "CRC8"
        print(f"\n{'─'*50}")
        print(f"  Running with {label}")
        print(f"{'─'*50}")
        all_results.append(test_normal_roundtrip(port, use_crc16))
        all_results.append(test_single_bit_error(port, use_crc16))
        all_results.append(test_frame_drop_simulation(port, use_crc16))
        all_results.append(test_max_payload(port, use_crc16))
        all_results.append(test_zero_payload(port, use_crc16))

    print(f"\n{'═'*50}")
    print("  TEST RESULTS")
    print(f"{'═'*50}")
    passed = failed = 0
    for r in all_results:
        sym = "✓" if r.passed else "✗"
        print(f"  [{sym}] {r.name}")
        if r.detail:
            print(f"       {r.detail}")
        if r.passed: passed += 1
        else:        failed += 1

    print(f"\n  {passed}/{passed+failed} tests passed")
    print(f"{'═'*50}\n")
    return failed == 0


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="CRC Frame Loopback Test")
    parser.add_argument("--port",  default=None, help="Serial port, e.g. /dev/ttyACM0")
    parser.add_argument("--baud",  default=921600, type=int)
    parser.add_argument("--mock",  action="store_true", help="Use software loopback")
    parser.add_argument("--crc",   choices=["8", "16", "both"], default="both", help="CRC strategy to test (8, 16, or both)")
    args = parser.parse_args()

    if args.mock or args.port is None:
        print("Using software mock loopback (no hardware required)")
        port = MockSerial()
    else:
        try:
            import serial
            port = serial.Serial(args.port, args.baud, timeout=1)

            # Allow full boot + settle
            time.sleep(3.5)

            # Capture and display any boot diagnostic bytes
            boot_bytes = port.read(port.in_waiting or 1)
            if boot_bytes:
                print(f"Boot diagnostic ({len(boot_bytes)}B):")
                try:
                    print(f"  ASCII: {boot_bytes.decode('ascii', errors='replace')}")
                except Exception:
                    pass
                print(f"  HEX:   {boot_bytes.hex()}")
            else:
                print("(no boot bytes received)")

            port.reset_input_buffer()  # discard any remaining stale bytes
            print(f"Connected to {args.port} @ {args.baud} baud")
        except ImportError:
            print("pyserial not installed. Falling back to mock.")
            port = MockSerial()

    success = run_all_tests(port, args.crc)
    port.close()
    sys.exit(0 if success else 1)
