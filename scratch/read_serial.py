import serial
import time
import sys

port = '/dev/ttyACM0'
baud = 921600

print(f"Opening {port}...")
try:
    ser = serial.Serial(port, baud, timeout=1)
except Exception as e:
    print(f"Error opening port: {e}")
    sys.exit(1)

print("Listening for 5 seconds...")
start_time = time.time()
while time.time() - start_time < 5:
    if ser.in_waiting:
        data = ser.read(ser.in_waiting)
        print(f"Received raw data: {data}")
    time.sleep(0.1)

ser.close()
print("Done.")
