
import time
import sys
import signal
import ctypes
import math
import serial
from datetime import datetime
from pixy import pixy
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

##### defining PixyCam sensory variables
PIXY_MIN_X = 0
PIXY_MAX_X = 319
PIXY_MIN_Y = 0
PIXY_MAX_Y = 199

PIXY_X_CENTER = ((PIXY_MAX_X-PIXY_MIN_X) / 2)
PIXY_Y_CENTER = ((PIXY_MAX_Y-PIXY_MIN_Y) / 2)
PIXY_RCS_MIN_POS = 0
PIXY_RCS_MAX_POS = 1000
PIXY_RCS_CENTER_POS = ((PIXY_RCS_MAX_POS-PIXY_RCS_MIN_POS) / 2)
BLOCK_BUFFER_SIZE = 10

##### defining PixyCam motor variables
PIXY_RCS_PAN_CHANNEL = 0
PIXY_RCS_TILT_CHANNEL = 1

PAN_PROPORTIONAL_GAIN = 400
PAN_DERIVATIVE_GAIN = 300
TILT_PROPORTIONAL_GAIN = 500
TILT_DERIVATIVE_GAIN = 400

MAX_MOTOR_SPEED = 300#480
MIN_MOTOR_SPEED = -480

run_flag = 1

# 20ms time interval for 50Hz
dt = 20
# check timeout dt*3
timeout = 0.5
currentTime = datetime.now()
lastTime = datetime.now()
lastFire = lastTime

#### defining motor function variables
# 5% drive is deadband
deadband = 0.05 * MAX_MOTOR_SPEED
# totalDrive is the total power available
totalDrive = MAX_MOTOR_SPEED
# throttle is how much of the totalDrive to use [0~1]
throttle = 0
# differential drive level [0~1]
diffDrive = 0
# this is the drive level allocated for steering [0~1] dynamically modulate
diffDrive = 0
# this is the gain for scaling diffDrive
diffGain = 1
# this ratio determines the steering [-1~1]
bias = 0
# this ratio determines the drive direction and magnitude [-1~1]
advance = 0
# this gain currently modulates the forward drive enhancement
driveGain = 1
# body turning p-gain
h_pgain = 0.5
# body turning d-gain
h_dgain = 0

#### defining state estimation variables
# pixyViewV = 47
# pixyViewH = 75
# pixyImgV = 400
# pixyImgH = 640
# pixel to visual angle conversion factor (only rough approximation) (pixyViewV/pixyImgV + pixyViewH/pixyImgH) / 2
pix2ang_factor = 0.117
# reference object one is the pink earplug (~12mm wide)
refSize1 = 12
# reference object two is side post (~50mm tall)
refSize2 = 50
# this is the distance estimation of an object
objectDist = 0
# this is some desired distance to keep (mm)
targetDist = 100
# reference distance; some fix distance to compare the object distance with
refDist = 400

blocks = None

def handle_SIGINT(sig, frame):
    """
    Handle CTRL-C quit by setting run flag to false
    This will break out of main loop and let you close
    pixy gracefully
    """
    global run_flag
    run_flag = False

class Blocks(ctypes.Structure):
    """
    Block structure for use with getting blocks from
    pixy.get_blocks()
    """
    _fields_ = [
        ("type", ctypes.c_uint),
        ("signature", ctypes.c_uint),
        ("x", ctypes.c_uint),
        ("y", ctypes.c_uint),
        ("width", ctypes.c_uint),
        ("height", ctypes.c_uint),
        ("angle", ctypes.c_uint)
    ]

class ServoLoop(object):
    """
    Loop to set pixy pan position
    """
    def __init__(self, pgain, dgain):
        self.m_pos = PIXY_RCS_CENTER_POS
        self.m_prevError = 0x80000000L
        self.m_pgain = pgain
        self.m_dgain = dgain

    def update(self, error):
        if self.m_prevError != 0x80000000:
            vel = (error * self.m_pgain + (error - self.m_prevError) * self.m_dgain) >> 10
            self.m_pos += vel
            if self.m_pos > PIXY_RCS_MAX_POS:
                self.m_pos = PIXY_RCS_MAX_POS
            elif self.m_pos < PIXY_RCS_MIN_POS:
                self.m_pos = PIXY_RCS_MIN_POS
        self.m_prevError = error

# define pan loop
panLoop = ServoLoop(300, 500)


def setup():
    """
    One time setup. Inialize pixy and set sigint handler
    """
    global blocks
    pixy_init_status = pixy.pixy_init()
    if pixy_init_status != 0:
        print 'Error: pixy_init() [%d] ' % pixy_init_status
        pixy.pixy_error(pixy_init_status)
        return
    else:
        print "Pixy setup OK"
    blocks = pixy.BlockArray(BLOCK_BUFFER_SIZE)
    signal.signal(signal.SIGINT, handle_SIGINT)

killed = False

def loop():
    """
    Main loop, Gets blocks from pixy, analyzes target location,
    chooses action for robot and sends instruction to motors
    """
    global blocks, throttle, diffDrive, diffGain, bias, advance, turnError, currentTime, lastTime, objectDist, distError, panError_prev, distError_prev, panLoop, killed, lastFire

    if ser.in_waiting:
        print "Reading line from serial.."
        code = ser.readline().rstrip()
        print "Got IR code %s" % code
        killed = True
        #if code=="58391E4E" or code=="9DF14DB3" or code=="68B92":
        #    killed = True
        #
        #if code=="E4F74E5A" or code=="A8FA9FFD":
        #    killed = False

    if killed:
        print "I'm hit!"
        motors.setSpeeds(0, 0)
        time.sleep(5)

    currentTime = datetime.now()
    # If no new blocks, don't do anything
    while not pixy.pixy_blocks_are_new() and run_flag:
        pass
    count = pixy.pixy_get_blocks(BLOCK_BUFFER_SIZE, blocks)
    # If negative blocks, something went wrong
    if count < 0:
        print 'Error: pixy_get_blocks() [%d] ' % count
        pixy.pixy_error(count)
        sys.exit(1)
    # if more than one block
    # Check which the largest block's signature and either do target chasing or
    # line following
    if count > 0:

        time_difference = currentTime - lastFire
        if time_difference.total_seconds() >= 1:
            print "Fire!"
            ser.write("FIRE\n")
            lastFire = currentTime

        lastTime = currentTime
        # if the largest block is the object to pursue, then prioritize this behavior
        if blocks[0].signature == 1:
            panError = PIXY_X_CENTER - blocks[0].x
            objectDist = refSize1 / (2 * math.tan(math.radians(blocks[0].width * pix2ang_factor)))
            throttle = 0.5
            # amount of steering depends on how much deviation is there
            diffDrive = diffGain * abs(float(panError)) / PIXY_X_CENTER
            distError = objectDist - targetDist
            # this is in float format with sign indicating advancing or retreating
            advance = driveGain * float(distError) / refDist
        # if Pixy sees a guideline, perform line following algorithm
        elif blocks[0].signature == 2:
            panError = PIXY_X_CENTER-blocks[0].x
            throttle = 1.0
            diffDrive = 0.6
            # amount of steering depends on how much deviation is there
            # diffDrive = diffGain * abs(float(turnError)) / PIXY_X_CENTER
            # use full available throttle for charging forward
            advance = 1            
        # if none of the blocks make sense, just pause
        else:
            panError = 0
            throttle = 0.0
            diffDrive = 1
        panLoop.update(panError)

    # Update pixy's pan position
    pixy.pixy_rcs_set_position(PIXY_RCS_PAN_CHANNEL, panLoop.m_pos)

    # if Pixy sees nothing recognizable, don't move.
    time_difference = currentTime - lastTime
    if time_difference.total_seconds() >= timeout:
        throttle = 0.0
        diffDrive = 1

    # this is turning to left
    if panLoop.m_pos > PIXY_RCS_CENTER_POS:
        # should be still int32_t
        turnError = panLoop.m_pos - PIXY_RCS_CENTER_POS
        # <0 is turning left; currently only p-control is implemented
        bias = - float(turnError) / float(PIXY_RCS_CENTER_POS) * h_pgain
    # this is turning to right
    elif panLoop.m_pos < PIXY_RCS_CENTER_POS:
        # should be still int32_t
        turnError = PIXY_RCS_CENTER_POS - panLoop.m_pos
        # >0 is turning left; currently only p-control is implemented
        bias = float(turnError) / float(PIXY_RCS_CENTER_POS) * h_pgain
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
        pixy.pixy_close()
        motors.setSpeeds(0, 0)
        print "Robot Shutdown Completed"



