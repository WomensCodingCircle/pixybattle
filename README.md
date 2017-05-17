# Pixy Battle

Welcome to the Pixy Battle!

More information can be found on the [official website](http://womenscodingcircle.com/pixyrace/).

## Initial Setup
If you want to return your PixyBot to its default settings, use one of the following methods:

### Method #1: PiBakery

The easiest way to reset your PixyBot is to use one of the PiBakery recipes provided under the ```recipes``` directory.

Download and run [PiBakery](http://www.pibakery.org/) and import the appropriate recipe. You can customize the recipe before flashing your SD card. If you plan to use wifi, you should customize the settings (e.g. fill in the password), but it's best to use an ethernet connection with no wifi (as shown in ```Pixy_Pi_Recipe_NoWifi.xml```) because the install downloads a lot of packages, and it may be slower over wifi.

Insert your PixyBot's SD card into your SD card writer and click "Write". 

Once the card is written, you can insert it back into the PixyBot and boot it up. The install can take up to an hour to download and install all the necessary libraries. If it is interrupted before completion, you can re-run the first boot script like this:

```
$ sudo /boot/PiBakery/firstBoot.sh
```

If you run into any trouble, the ```/boot/PiBakery``` directory also contains logs which can explain what went wrong.

### Method #2: Manual Install

With this alternative method, you will perform the installation steps manually. This has the benefit of being able to check at each step to make sure everything has gone smoothly, as well as being a chance to get familiar with all the dependencies and libraries.

#### Download NOOBS and flash the SD card
Start by [downloading the NOOBS image](https://www.raspberrypi.org/downloads/noobs/) and copying it to the SD card. TO format the SD card, you can use utilities built into your operating system, or download a tool like [SD Formatter](https://www.sdcard.org/downloads/formatter_4/). 

#### Boot
Once the SD card is ready, insert it into the Raspberry Pi, and connect an HDMI cable, a keyboard, and a laptop. Then connect a power cable to boot the Pi. The adapter should be capable of at least 2A @ 5V.

#### Install dependencies 
Open a terminal window and type the following command to download and install the necessary system packages:
```
$ sudo apt-get install git libusb-1.0-0-dev \
    qt4-dev-tools g++ libboost-all-dev cmake swig
```

#### Clone this git repository
```
$ git clone https://github.com/WomensCodingCircle/pixybattle.git
```

#### Run the installer
This will download and build all the required libraries, and install them in the appropriate locations.
```
$ cd pixybattle
$ ./install.sh
```

## Using SSH
We recommend enabling SSH, so that you can connect to the Pi remotely:
1. Enter `sudo raspi-config` in a terminal window
2. Select **Interfacing Options**
3. Navigate to and select **SSH**
4. Choose **Yes**
5. Select **Ok**
6. Choose **Finish**

If you have a Macbook, you can now connect directly to the Pi with an ethernet cable, and then run ssh from Terminal:
```
$ ssh pi@raspberrypi.local
```
Other operating systems require [some extra effort](https://pihw.wordpress.com/guides/direct-network-connection/).

## Using PixyMon
In order to assign color signatures for the PixyCam to recognize, you must use a utility called **PixyMon**. To run this utility, connect an HDMI monitor, keyboard, and mouse to the Raspberry Pi and log into the GUI environment. If you followed all the setup steps above, you should see a shortcut to PixyMon on your desktop. 

## Running the code

Once the initial set up is complete, you should be able to run any of the scripts provided in ```src/python```:

* **racer.py** - this is the basic line-following racer code from 2016's Pixy Race
* **circle.py** - randomly roams about around and pauses for 5 seconds each time it gets hit with an opposing IR gun
* **lasertag.py** - follows Pixy's signature #1 (as set in PixyMon) and tries to fire the IR gun every second

You must run these scripts as root (i.e. using ```sudo```), for example:

```
$ cd ~/pixybattle/src/python
$ sudo python lasertag.py
```

## Testing with the Round Targets

The targets communicate over a serial protocol. To execute commands on the target, plug it into a USB port and then open a connection using a serial communication program. There are several options, depending on your operating system. In either case you should install the drivers first:

1) Install [Arduino IDE](https://www.arduino.cc/en/Main/Software)
2) Install [Teensyduino](https://www.pjrc.com/teensy/td_download.html)

At this point, you can try using the Serial Monitor built into Arduino IDE, or you can use a dedicated RS232 terminal:

### Serial Monitor

Open Arduino IDE, and make the following selections in the menus:
Tools->Board->Teensy 3.2
Tools->USB Type->Serial
Tools->Port->(choose Teensy port)

When you type **HELP** you should get this response from the Target:
```
Monitor communications:
TX:
  NEUTRAL,<LEFT/RIGHT>                   --> when going to default (green)
  HIT,<RED/BLUE,<LEFT/RIGHT>             --> when getting 1st or 2nd hit
  <RED/BLUE>:#1st hits,#2nd hits,#final  --> upon receiving 'SCORE' command
RX:
  START          --> starts the game
  STOP           --> stops the game
  SCORE          --> sends total hit counts
  TEST_RED       --> sets both sides to red
  TEST_BLUE      --> sets both sides to blue
  TEST_GREEN     --> sets both sides to green
  TEST_RED_BLUE  --> sets one side to red, one side to blue
  RESET          --> reset target (ready to receive 'START')
  HELP           --> list of commands
```

### Windows

[Termite](https://www.compuphase.com/software_termite.htm) will also work for serial communication. After running it, click on **Settings** and then select the Port that your Target is connected to. You should also change **Transmitted text** to "Append CR-LF". 

### MacOS

On Macs, you can open the default Terminal program and then type:
```
$ ls -l /dev/cu.usb*
```

This will show your current USB devices. Identify the one which represents your connected Target and then type, for example:
```
$ screen -L /dev/cu.usbmodem2863271 115200
```

## FAQs

### My PixyBot keeps rebooting, what gives?

This is usually due to power issues. If you are running off batteries, make sure they are not drained. If you have power plugged into the micro USB port, make sure that it is capable of supplying 5V @ 2A. Do __not__ plug the Pi into a regular USB data port.

### Things are not working as expected after my PiBakery install

After booting, open a terminal (or ssh) and type:
```
$ more /boot/PiBakery/firstboot.log
```
This will show you a log of what happened during the first boot, during which PiBakery should have installed all the software and made the configuration setting changes. 

If you'd like to run the PiBakery first boot script again, you can do so like this:
```
$ sudo cp /boot/PiBakery/firstBoot.sh /boot/PiBakery/nextBoot.sh
```
This will force the setup commands to run again on your next boot.

### I'm getting errors when installing things with apt-get

This might be due to the package site having problems. You can try a mirror by editing the sources file:
```
$ vi /etc/apt/sources.list
```
Now change the URL in the line beginning with "deb" to another package site found in the [mirror list](https://www.raspbian.org/RaspbianMirrors).

