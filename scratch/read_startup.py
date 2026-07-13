import serial
import time
import sys

port = '/dev/ttyACM0'
baud = 921600

try:
    ser = serial.Serial(port, baud, timeout=1)
except Exception as e:
    print(f"Error opening port: {e}")
    sys.exit(1)

print("Listening for 5 seconds...")
start_time = time.time()
buffer = b""
while time.time() - start_time < 5:
    if ser.in_waiting:
        data = ser.read(ser.in_waiting)
        buffer += data
    time.sleep(0.1)

print(f"Done. Received: {buffer!r}")
ser.close()
