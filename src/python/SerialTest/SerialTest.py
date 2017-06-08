# -*- coding: utf-8 -*-
"""
Created on Wed Jun  7 05:55:03 2017

@author: Cameron

Based on: https://stackoverflow.com/questions/12090503/listing-available-com-ports-with-python

To get a list of COM ports
python -m serial.tools.list_ports

Needs Arduino installed (to get USB drivers)
"""

import sys
import glob
import serial
import io
import time



def serial_ports():
    """ Lists serial port names

        :raises EnvironmentError:
            On unsupported or unknown platforms
        :returns:
            A list of the serial ports available on the system
    """
    if sys.platform.startswith('win'):
        ports = ['COM%s' % (i + 1) for i in range(256)]
    elif sys.platform.startswith('linux') or sys.platform.startswith('cygwin'):
        # this excludes your current terminal "/dev/tty"
        ports = glob.glob('/dev/tty[A-Za-z]*')
    elif sys.platform.startswith('darwin'):
        ports = glob.glob('/dev/tty.*')
    else:
        raise EnvironmentError('Unsupported platform')

    result = []
    for port in ports:
        try:
            s = serial.Serial(port)
            s.close()
            result.append(port)
        except (OSError, serial.SerialException):
            pass
    return result



def serial_open(portID):
    ser = serial.Serial()
    ser.baudrate = 115200
    ser.port = portID
    ser.timeout = 0.1
    ser.write_timeout = 1   
    ser.open()
    return ser 


def serial_close(ser):
    ser.close()

def serial_send_text(ser, textToSend):   
    ser.write(textToSend.decode("utf-8"),b'\r\n')
    ser.flush() # it is buffering. required to get the data out *now*
    
def serial_read_text(ser):
    return str(ser.readline().rstrip(),"utf-8")
        
def target_start(ser):
    ser.write(b'START\r\n')
    ser.flush() # it is buffering. required to get the data out *now*

def target_reset(ser):
    ser.write(b'RESET\r\n')    
    ser.flush() # it is buffering. required to get the data out *now*

def target_stop(ser):
    ser.write(b'STOP\r\n')     
    ser.flush() # it is buffering. required to get the data out *now*

def target_get_version(ser):   
    ser.write(b'VERSION\r\n')
    ser.flush()
    return str(ser.readline().rstrip(),"utf-8") 
  
def target_get_score(ser):
    ser.write(b'SCORE\r\n')
    scoreRed = str(p.readline().rstrip(),"utf-8")
    scoreBlue = str(p.readline().rstrip(),"utf-8")
    return scoreRed, scoreBlue

#open all ports
#start all targets
#wait 3 minutes and periodically display score
#stop all targets
#get final score
#close all ports    
if __name__ == '__main__':
    portsList = serial_ports()    
    print("Available: ", portsList) 
    
    #remove COM1 (first port) for WINDOWS OS
    if sys.platform.startswith('win'):    
        portsList.pop(0)

    #open all COM ports
    portStream = []
    ports = []
    for p in portsList:
        print("Using: ", p)    
        ser = serial_open(p)
        ports.append(ser)

    print()
    
    #start all targets    
    for p in ports:    
        target_reset(p)
        target_start(p)   
        time.sleep(1);
        p.reset_input_buffer();
        #target_get_version(ser)
        #serial_send_text(ser, "HELP")
        #serial_read_text(ser)  
      
    #wait 3 minutes and periodically display score
    timeStart = time.clock() 
    timeStop = timeStart + 10.0 #CHANGE TO: 3 * 60.0
    while (time.clock() < timeStop):
        print(time.clock() - timeStart)
        
        for p in ports:
            print (target_get_score(p))

        print()        
        time.sleep(1);
    
    #stop all targets 
    for p in ports:    
        target_stop(p)
        
    #get final score
    for p in ports:  
        print("Target on port: ", p.port)
        print (target_get_score(p))    

    #close all COM ports
    for p in ports:       
        serial_close(p)
    