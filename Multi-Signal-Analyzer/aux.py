import serial
import struct
import time
import threading

ports = {
    '/dev/ttyACM1': 115200,
    '/dev/ttyACM2': 115200,
    '/dev/ttyACM3': 115200,
    '/dev/ttyACM4': 115200,
    '/dev/ttyACM5': 115200,
    '/dev/ttyACM6': 115200,
    '/dev/ttyACM7': 115200,
    '/dev/ttyACM8': 115200,
}

cmd = {
    'RESET': 0x00,
    'GET_INFO': 0x01,
    'ARM_CAPTURE': 0x10,
    'SET_CHANNELS': 0x12,
}

digital_chan = 0xffff
analog_chan = 0xff
channel_mask = (digital_chan << 8) | analog_chan

def get_infos(ser):
    packet = cmd['GET_INFO']
    packet = packet.to_bytes(1, 'little')
    
    ser.write(packet)
    
    data = ser.readline().decode('utf-8',errors='ignore').strip() 
    print("INFOS:", data)

def set_channels(ser, mask):
    packet = (cmd['SET_CHANNELS'] << (8*3)) | channel_mask
    packet = packet.to_bytes(4, 'big')
    
    ser.write(packet)
    
    data = ser.readline().decode('utf-8',errors='ignore').strip() 
    data = data.replace('_', ' ')
    print('SET CHANNELS RESPONSE: [{}]'.format(data))
    
def arm_capture(ser):
    packet = cmd['ARM_CAPTURE']
    ser.write(packet.to_bytes(1, 'little'))


def serial_listener(ser):
    while True:
        time.sleep(5)
        arm_capture(ser)
        data = ser.readline().decode('utf-8',errors='ignore').strip() 
        data = data.replace('_', ' ')
        if data:
            print('DATA RECEIVED: [{}]'.format(data))
            

def open_first_available_port():
    for i in ports:
        try:
            ser = serial.Serial(i, ports[i], timeout=0.1)
            if ser.is_open:
                print(f"Conectado na porta {i}")
                return ser
        except serial.SerialException:
            continue
    raise serial.SerialException("Nenhuma porta serial disponível.")

def __main__():
    try:
        ser = open_first_available_port()
        
        if ser.is_open:
                print(f"Conectado na porta {ser.port}")

        get_infos(ser)
        
        set_channels(ser, channel_mask)
        
        arm_capture(ser)
        
        time.sleep(1)
        
        listener_thread = threading.Thread(target=serial_listener, args=(ser,), daemon=True)
        listener_thread.start()
        
        while True:
            arm_capture(ser)
            # time.sleep(1)   

    except serial.SerialException as e:
        pass

    except KeyboardInterrupt:
        if 'ser' in locals() and ser.is_open:
            ser.close()
            print("Porta serial fechada.")

    finally:
        if 'ser' in locals() and ser.is_open:
            ser.close()
            print("Porta serial fechada.")
            
if __name__ == "__main__":
    __main__()