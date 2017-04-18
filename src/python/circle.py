
import time
import sys
import signal
import ctypes
import math
import serial
from datetime import datetime
from pololu_drv8835_rpi import motors

serialDevice = '/dev/ttyACM0'
baudRate = 9600

while True:
    try:
        ser = serial.Serial(serialDevice, baudRate)
        break
    except:
        print "Could not open serial device %s"%serialDevice
        time.sleep(10)

MAX_MOTOR_SPEED = 300#480
MIN_MOTOR_SPEED = -480

run_flag = 1

# 20ms time interval for 50Hz
dt = 20
# check timeout dt*3
timeout = 0.5
currentTime = datetime.now()
lastTime = datetime.now()


#### defining motor function variables
# 5% drive is deadband
deadband = 0.05 * MAX_MOTOR_SPEED
# totalDrive is the total power available
totalDrive = MAX_MOTOR_SPEED
# throttle is how much of the totalDrive to use [0~1]
throttle = 0.7
# differential drive level [0~1]
# this is the drive level allocated for steering [0~1] dynamically modulate
diffDrive = 0.3
# this is the gain for scaling diffDrive
diffGain = 1
# this ratio determines the steering [-1~1]
bias = 1
# this ratio determines the drive direction and magnitude [-1~1]
advance = 1
# this gain currently modulates the forward drive enhancement
driveGain = 1
# body turning p-gain
h_pgain = 0.5
# body turning d-gain
h_dgain = 0

def handle_SIGINT(sig, frame):
    """
    Handle CTRL-C quit by setting run flag to false
    This will break out of main loop and let you close
    pixy gracefully
    """
    global run_flag
    run_flag = False


def setup():
    signal.signal(signal.SIGINT, handle_SIGINT)

killed = False

def loop():
    """
    Main loop, Gets blocks from pixy, analyzes target location,
    chooses action for robot and sends instruction to motors
    """
    global throttle, diffDrive, diffGain, bias, advance, turnError, currentTime, lastTime, objectDist, distError, panError_prev, distError_prev, panLoop, killed

    if ser.in_waiting:
        print "reading line from serial.."
        code = ser.readline().rstrip()
        print "Got IR code %s" % code
        killed = True

    if killed:
        motors.setSpeeds(0, 0)
        time.sleep(5)

    currentTime = datetime.now()
    drive()
    return run_flag

def drive():
    # synDrive is the drive level for going forward or backward (for both wheels)
    synDrive = advance * (1 - diffDrive) * throttle * totalDrive
    leftDiff = bias * diffDrive * throttle * totalDrive
    rightDiff = -bias * diffDrive * throttle * totalDrive

    # construct the drive levels
    LDrive = (synDrive + leftDiff)
    RDrive = (synDrive + rightDiff)

    # Make sure that it is outside dead band and less than the max
    if LDrive > deadband:
        if LDrive > MAX_MOTOR_SPEED:
            LDrive = MAX_MOTOR_SPEED
    elif LDrive < -deadband:
        if LDrive < -MAX_MOTOR_SPEED:
            LDrive = -MAX_MOTOR_SPEED
    else:
        LDrive = 0

    if RDrive > deadband:
        if RDrive > MAX_MOTOR_SPEED:
            RDrive = MAX_MOTOR_SPEED
    elif RDrive < -deadband:
        if RDrive < -MAX_MOTOR_SPEED:
            RDrive = -MAX_MOTOR_SPEED
    else:
        RDrive = 0

    # Actually Set the motors
    motors.setSpeeds(int(LDrive), int(RDrive))

if __name__ == '__main__':
    try:
        setup()
        while True:
            ok = loop()
            if not ok:
                break
    finally:
        motors.setSpeeds(0, 0)
        print "Robot Shutdown Completed"



