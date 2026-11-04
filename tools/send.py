import serial,sys,time
s=serial.Serial('/dev/cu.usbmodem14301',115200,timeout=0.3)
time.sleep(2); print(s.read(4000).decode(errors='replace'))
for expr in sys.argv[1:]:
    s.write((expr+'\n').encode()); time.sleep(1.5)
    print(s.read(8000).decode(errors='replace'))
