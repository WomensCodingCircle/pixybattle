import sys
import traceback
import signal
import ctypes
import math
import time
from datetime import datetime
import argparse

from pixy import pixy
from pololu_drv8835_rpi import motors

# Libraries for playing sound on the web service
import requests
import threading
from Queue import Queue

#VOICE_SERVICE_URL = "http://localhost:8080"
VOICE_SERVICE_URL = "http://10.9.6.2:8080"

BRIGHTNESS = 185

#### signature ids ####
OBSTACLE = 1
CENTER_LINE = 2
LEFT_LINE = 3
RIGHT_LINE = 4
L_POST = 5
R_POST = 6

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

MAX_MOTOR_SPEED = 480
MIN_MOTOR_SPEED = -480

AVG_N = 3

run_flag = 1
firstPass = True
startTime = time.time()

# options that can be set by command-line arguments
no_brightness_check = True 
chatty = False
allow_move = True
finale = False

initThrottle = 1.0 #0.9
diffDriveStraight = 0.4 #0.6
diffDrivePosts = 0.5 #0.6

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
throttle = 0
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
h_pgain = 0.7
# body turning d-gain
h_dgain = 0.2

# turn error
turnError = 0
# PID controller
pid_bias = 0
last_turn = 0

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

sayingQueue = Queue()

def sayNow(saying):
    if not chatty:
        return
    try:
        if saying.startswith("SLEEP"):
            requests.get('%s/say?sleep=%s' % (VOICE_SERVICE_URL, saying.split()[1]))
        else:
            requests.get('%s/say?text=%s' % (VOICE_SERVICE_URL, saying))
    except Exception, err:
        print "Couldn't send saying to voice server",err

def say(saying):
    print "Saying '%s'" % saying
    if not chatty:
        return
    sayingQueue.put(saying)

def voiceThreadLoop():
    def webSay(saying):
        try:
            if saying.startswith("SLEEP"):
                requests.get('%s/say?sleep=%s' % (VOICE_SERVICE_URL, saying.split()[1]))
            else:
                requests.get('%s/say?text=%s' % (VOICE_SERVICE_URL, saying))
        except Exception, err:
            print "Couldn't send saying to voice server",err
    lastSaying = None
    lastTime = datetime.now()
    while True:
        saying = sayingQueue.get()
        currentTime = datetime.now()
        time_difference = currentTime - lastTime
        if saying:
            if not(saying==lastSaying) or time_difference.total_seconds() >= 3:
                webSay(saying)
                lastSaying = saying
                lastTime = currentTime

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

class PID:
    """
    Discrete PID control
    """
    def __init__(self, P=2.0, I=0.0, D=1.0, Derivator=0, Integrator=0, Integrator_max=500, Integrator_min=-500):
        self.Kp=P
        self.Ki=I
        self.Kd=D
        self.Derivator=Derivator
        self.Integrator=Integrator
        self.Integrator_max=Integrator_max
        self.Integrator_min=Integrator_min
        self.set_point=0.0
        self.error=0.0

    def update(self,current_value):
        """
        Calculate PID output value for given reference input and feedback
        """
        self.error = self.set_point - current_value
        P_value = self.Kp * self.error
        D_value = self.Kd * ( self.error - self.Derivator)
        self.Derivator = self.error
        self.Integrator = self.Integrator + self.error
        if self.Integrator > self.Integrator_max:
                self.Integrator = self.Integrator_max
        elif self.Integrator < self.Integrator_min:
                self.Integrator = self.Integrator_min
        I_value = self.Integrator * self.Ki
        PID = P_value + I_value + D_value
        # print "SP:%2.2f PV:%2.2f Error:%2.2f -> P:%2.2f I:%2.2f D:%2.2f" % (self.set_point, current_value, self.error, P_value, I_value, D_value)
        return PID

    def setPoint(self,set_point):
        """
        Initilize the setpoint of PID
        """
        self.set_point = set_point
        self.Integrator=0
        self.Derivator=0

pid = PID(h_pgain, 0, 0)

# logic for horizon per signature, etc.
def ignore(block):
    above_horizon = block.y < 60
    lines = (block.signature == LEFT_LINE) or (block.signature == CENTER_LINE) or (block.signature == RIGHT_LINE)
    if lines and above_horizon:
        return True
    return False

def adjust_brightness(bright_delta):
    pass

class Scene(object):
    """
    Detects different objects in a Scene.
    """
    def __init__(self):
        self.m_blocks = pixy.BlockArray(BLOCK_BUFFER_SIZE)
        self.m_blockCount = []
        self.m_panError = 0
        self.m_brightness = BRIGHTNESS
        self.m_count = 0

    def is_sufficient(self):
        if not self.m_blockmap:
            return False
        # Should also check if we can see center or posts as backup.
        if self.seeCenter():
            return True
        return False

    def get_blocks(self):
        self.m_count = pixy.pixy_get_blocks(BLOCK_BUFFER_SIZE, self.m_blocks)

        # If negative blocks, something went wrong
        if self.m_count < 0:
            print 'Error: pixy_get_blocks() [%d] ' % self.m_count
            pixy.pixy_error(self.m_count)
            sys.exit(1)
        if self.m_count == 0:
            print "Detected no blocks"
            return None

        # package per signature
        i = 0
        blockmap = {}
        for block in self.m_blocks:
            if ignore(block):
                continue
            if block.signature not in blockmap:
                blockmap[block.signature] = []
            blockmap[block.signature].append(block)
            if i >= self.m_count:
                break
            i += 1
        return blockmap

    def blocksSeen(self):
        return self.m_count > 0

    def postsSeen(self):
        if self.m_blockmap == None:
            return False
        lpost = L_POST in self.m_blockmap
        rpost = R_POST in self.m_blockmap
        return lpost or rpost

    def seeCenter(self):
        if self.m_blockmap == None:
            return False
        return CENTER_LINE in self.m_blockmap

    def setPanError(self):
        if self.m_count == 0 or not (CENTER_LINE in self.m_blockmap):
            self.m_panError = 0
            return
        center = self.m_blockmap[CENTER_LINE]
        if len(center) > 1:
            self.m_panError = PIXY_X_CENTER-self.m_blockmap[CENTER_LINE][1].x
        else:
            self.m_panError = PIXY_X_CENTER-self.m_blockmap[CENTER_LINE][0].x


    @property
    def panError(self):
        return self.m_panError

    def get_frame(self):
        """Populates panError, blockCount, and blocks for a frame"""

        self.m_blockmap = None

        if no_brightness_check:
            self.m_blockmap = self.get_blocks()
            self.setPanError()
        else:
            self.m_brightness = pixy.pixy_cam_get_brightness()
            bmax = self.m_brightness + 20
            if bmax > 255:
                bmax = 255
            bmin = self.m_brightness - 20
            if bmin < 60:
                bmin = min(self.m_brightness-1, 60)
            gotit = False
            for i in range(self.m_brightness, bmax):
                self.m_blockmap = self.get_blocks()
                if self.is_sufficient():
                    gotit = True
                    break
                pixy.pixy_cam_set_brightness(i+1)
            if not gotit:
                for i in range(self.m_brightness-1, bmin, -1):
                    self.m_blockmap = self.get_blocks()
                    if self.is_sufficient():
                        gotit = True
                        break
                    pixy.pixy_cam_set_brightness(i)

            self.setPanError()
            if gotit:
                self.m_brightness = pixy.pixy_cam_get_brightness()
                print "Got good signtures at brightness %d" % self.m_brightness
            else:
                print "Could not find good signatures after brightness changes!"
                pixy.pixy_cam_set_brightness(self.m_brightness)
                return

        # calculate center blocks on each side
        right = 0
        left = 0
        if not self.m_blockmap:
            return
        if CENTER_LINE in self.m_blockmap:
            for block in self.m_blockmap[CENTER_LINE]:
                #print "Counting center block at %d" % block.x
                if block.x > PIXY_X_CENTER:    #should look for center of car, not center of pixycam view.  
                    right += 1
                else:
                    left += 1

        #print "Center blocks: left=%d, right=%d" % (left, right)

        # keep track of past AVG_N red blocks
        if len(self.m_blockCount) > AVG_N:
            self.m_blockCount.pop()
        self.m_blockCount.insert(0, (left, right))



# init object processing
scene = Scene()

def setup():
    """
    One time setup. Inialize pixy and set sigint handler
    """
    pixy_init_status = pixy.pixy_init()
    if pixy_init_status != 0:
        print 'Error: pixy_init() [%d] ' % pixy_init_status
        pixy.pixy_error(pixy_init_status)
        return
    else:
        print "Pixy setup OK"
    signal.signal(signal.SIGINT, handle_SIGINT)
    pixy.pixy_cam_set_brightness(BRIGHTNESS)
    pixy.pixy_rcs_set_position(PIXY_RCS_PAN_CHANNEL, PIXY_RCS_CENTER_POS)
    
    if chatty:
        sayNow("I may not be the fastest but I have style")
        #say("SLEEP 2")
        time.sleep(2)

def loop():
    """
    Main loop, Gets blocks from pixy, analyzes target location,
    chooses action for robot and sends instruction to motors
    """
    global startTime, throttle, diffDrive, diffGain, bias, advance, turnError, currentTime, lastTime, objectDist, distError, panError_prev, distError_prev, firstPass, pid_bias, last_turn

    currentTime = datetime.now()
    # If no new blocks, don't do anything
    while not pixy.pixy_blocks_are_new() and run_flag:
        pass

    if firstPass:
        say("Here goes")
        startTime = time.time()
        firstPass = False

    scene.get_frame()
    if scene.blocksSeen():
        lastTime = currentTime
        
    if finale:
        refuseToPlay()
        return False

    p = scene.panError
    if p < 0:
        p = -p
    incr = p / 300.0
    #print "panError: %f, incr: %f" % (scene.panError, incr)
    #if incr > 0.65:
    #    incr = 0.65
    throttle = initThrottle  # - incr / 1.5
    diffDrive = diffDriveStraight + incr

    # amount of steering depends on how much deviation is there
    #diffDrive = diffGain * abs(float(turnError)) / PIXY_X_CENTER
    # use full available throttle for charging forward
    advance = 1

    panLoop.update(scene.panError)

    # Update pixy's pan position
    pixy.pixy_rcs_set_position(PIXY_RCS_PAN_CHANNEL, panLoop.m_pos)

    # if Pixy sees nothing recognizable, don't move.
    # time_difference = currentTime - lastTime
    if not scene.seeCenter(): #time_difference.total_seconds() >= timeout:
        print "Stopping since see nothing"
        throttle = 0.0
        diffDrive = 1

    turn = 0

    # this is turning to left
    if panLoop.m_pos > PIXY_RCS_CENTER_POS:
        # should be still int32_t
        turnError = panLoop.m_pos - PIXY_RCS_CENTER_POS
        # <0 is turning left; currently only p-control is implemented
        turn = float(turnError) / float(PIXY_RCS_CENTER_POS)

    # this is turning to right
    elif panLoop.m_pos < PIXY_RCS_CENTER_POS:
        # should be still int32_t
        turnError = PIXY_RCS_CENTER_POS - panLoop.m_pos
        # >0 is turning left; currently only p-control is implemented
        turn = -float(turnError) / float(PIXY_RCS_CENTER_POS)

    pid.setPoint(0)
    pid_bias = pid.update(turn)
    #print "PID controller: SP=%2.2f PV=%2.2f -> OP=%2.2f" % (0, turn, pid_bias)
    last_turn = turn
    bias = pid_bias # use PID controller on turn bias
    # TODO: parameterize drive()

    if bias < -0.3:
        say("Going left")
    if bias > 0.3:
        say("Going right")

    drive()
    return run_flag

def drive():

    if not allow_move:
        return
    
    if advance < 0:
        say("Backup up.  Beep.  Beep.  Beep.")
        print "Drive: Backing up.  Beeeep...Beeeep...Beeeep"

    #print "Drive: advance=%2.2f, throttle=%2.2f, diffDrive=%2.2f, bias=%2.2f" % (advance, throttle, diffDrive, bias)

    # synDrive is the drive level for going forward or backward (for both wheels)
    synDrive = advance * (1 - diffDrive) * throttle * totalDrive
    leftDiff = bias * diffDrive * throttle * totalDrive
    rightDiff = -bias * diffDrive * throttle * totalDrive
    #print "Drive: synDrive=%2.2f, leftDiff=%2.2f, rightDiff=%2.2f" % (synDrive, leftDiff, rightDiff)

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

### Dance moves

def forward(t):
    global bias, advance, throttle, diffDrive
    bias =0
    advance =1
    throttle =.25
    diffDrive=0
    drive()
    time.sleep(t) 

def backward(t):
    global bias, advance, throttle, diffDrive
    bias =0
    advance= -1
    throttle=.3
    diffDrive=0
    drive()
    time.sleep(t)

def r_spin(t):
    global bias, advance, throttle, diffDrive
    bias =1
    advance=1
    throttle=.3
    diffDrive=1
    drive()
    time.sleep(t)

def l_spin(t):
    global bias, advance, throttle, diffDrive
    bias = -1
    advance=1
    throttle=.3
    diffDrive=1
    drive()
    time.sleep(t)

def right(t):
    global bias, advance, throttle, diffDrive
    bias =0.5
    advance=1
    throttle=.4
    diffDrive=.5
    drive()
    time.sleep(t)

def left(t):
    global bias, advance, throttle, diffDrive
    bias = -0.5
    advance=1
    throttle=.3
    diffDrive=.5
    drive()
    time.sleep(t)

###  Experimental behaviors

def refuseToPlay():
        motors.setSpeeds(0, 0)
        l_spin(1)
        
        sayNow("I'm going to dance")
        #say("SLEEP 1")
        time.sleep(2)
 
        right(1.5)
        left(1.5)
        forward(2)
        backward(2)
        r_spin(4)
        l_spin(3)
        r_spin(3)
        right(1.5)
        left(1.5)
        backward(2)
        forward(2)
        r_spin(5)
        l_spin(5)
        #while True:
        #    ok = loop()
        #    if not ok:
        #        break
        #pixy.pixy_close()
        motors.setSpeeds(0, 0)

    

def backup():
    panError = PIXY_X_CENTER - blocks[0].x
    objectDist = refSize1 / (2 * math.tan(math.radians(blocks[0].width * pix2ang_factor)))
    throttle = 0.5
    # amount of steering depends on how much deviation is there
    diffDrive = diffGain * abs(float(panError)) / PIXY_X_CENTER
    distError = objectDist - targetDist
    # this is in float format with sign indicating advancing or retreating
    advance = driveGain * float(distError) / refDist


def hailmary(block_count):
    global throttle, diffDrive, diffGain, bias, advance, turnError, currentTime, lastTime, objectDist, distError, panError_prev, distError_prev
    if len(block_count) == 0:
        print "can't do hailmary with no blocks"
        return

    print "Attempting to hail Mary with %d block history"%len(block_count)
    leftTotal = 0.0
    rightTotal = 0.0
    for (left, right) in block_count:
        leftTotal += left
        rightTotal += right
    avgLeft = leftTotal / len(block_count)
    avgRight = rightTotal / len(block_count)
    print "Past %d frames had avg red blocks on (left=%d, right=%d)" % (AVG_N,avgLeft,avgRight)
    # Turn towards the preponderance of red blocks
    lastTime = currentTime
    if avgLeft>avgRight:
        print "Executing blind left turn"
        bias = -1
    elif avgRight>avgLeft:
        print "Executing blind right turn"
        bias = 1
    else:
        bias = 0
    if bias != 0:
        # Slow forward turn
        advance = 1
        throttle = 0.5
        diffDrive = 1
        # Reset pixy's head
        pixy.pixy_rcs_set_position(PIXY_RCS_PAN_CHANNEL, PIXY_RCS_CENTER_POS)
        normalDrive = False
    #If hailmary didn't work, hold on to your rosary beads, we're going hunting!
    else:
        # Need some kind of hunting behavior here
        #This situation usally arises when we're on a curve in one direction and want to sharply turn in the other direction
        #pan the camera until we find a red block, then go straight toward it.  
        print "Execute search and destroy"
        i=0
        noRedBlocks=True
        advance=-1 #if we can't find it, retreat
        while (noRedBlocks==True and i <= PIXY_RCS_MAX_POS):
        #while redblock not found
            #pan for red block
            print "Panning for red block. i:%d" %(i)
            pixy.pixy_rcs_set_position(PIXY_RCS_PAN_CHANNEL, i)
            count = pixy.pixy_get_blocks(BLOCK_BUFFER_SIZE, blocks)
            largestBlock = blocks[0]
            #code stolen from earlier in file, maybe turn it into a function?
            if largestBlock.signature == 2:
                noRedBlocks=False
                panError = PIXY_X_CENTER-blocks[0].x
                p = panError / 40.0
                if p < 0:
                    p = -p
                if p > 0.8:
                    p = 0.8
                throttle = 0.9 - p
                diffDrive = 0.6
                print "p: %f, panError: %d, turnError: %d" % (p, panError, turnError)
                advance = 1
            i= i +10


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description='The Heather RaspberryPi Robot Racer')
    
    parser.add_argument('--chatty', dest='chatty', action='store_true')
    parser.set_defaults(chatty=False)    
    
    parser.add_argument('--bright', dest='bright', action='store_true')
    parser.set_defaults(bright=False)    

    parser.add_argument('--no-move', dest='move', action='store_false')
    parser.set_defaults(move=True)    

    parser.add_argument('--finale', dest='finale', action='store_true')
    parser.set_defaults(finale=False)    

    parser.add_argument("--lookahead", type=int, choices=[0, 1, 2],
                        help="set the center lookahead")
    parser.set_defaults(lookahead=0)    

    args = parser.parse_args()
    print "Chatty mode: ", args.chatty
    print "Alter brightness: ", args.bright
    print "Lookahead: ", args.lookahead
    
    if args.bright:
        no_brightness_check = False

    if not args.move:
        allow_move = False

    if args.finale:
        finale = True
        
    if args.chatty:
        chatty = True
        # Start thread to listen to say() commands and forward them to the text2speech web service
        t = threading.Thread(target=voiceThreadLoop)
        t.daemon = True
        t.start()
        
    # Robot set up 
    setup()
    # Main loop
    try:
        while True:
            ok = loop()
            if not ok:
                break
    finally:
        say("Good bye")
        pixy.pixy_close()
        motors.setSpeeds(0, 0)
        print "Robot Shutdown Completed"

