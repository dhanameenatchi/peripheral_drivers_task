import serial
import struct
import time

# CRC16 CCITT
def crc16_ccitt(data: bytes) -> int:
    crc = 0xFFFF
    for byte in data:
        crc ^= (byte << 8)
        for _ in range(8):
            if crc & 0x8000:
                crc = ((crc << 1) ^ 0x1021) & 0xFFFF
            else:
                crc = (crc << 1) & 0xFFFF
    return crc

# Frame config
SOF = 0xAA
cmd = 0x42
payload = b"ALARM_01"

# Encode
header = bytes([SOF, len(payload), cmd]) + payload
crc = crc16_ccitt(header)
frame = header + struct.pack(">H", crc)

print(f"Opening serial port...")
ser = serial.Serial('/dev/ttyACM0', 921600, timeout=2)
time.sleep(2)
ser.reset_input_buffer()

print(f"Sending frame (len={len(frame)}): {frame.hex()}")
ser.write(frame)

print("Waiting for response...")
resp = ser.read(100)
print(f"Received (len={len(resp)}): {resp.hex()}")

ser.close()
