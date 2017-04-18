# Pixy Battle

Welcome to the Pixy Battle!

More information can be found on the [official website](http://womenscodingcircle.com/pixyrace/).

## Raspberry Pi Setup
If you want to return your PixyBot to its default settings, follow one of the following methods:

### Method #1: 

The easiest way to reset your PixyBot is to use the PiBakery recipe provided under the ./recipes directory.

Download and run [PiBakery](http://www.pibakery.org/) and import the appropriate recipe. You can customize the Wifi settings as well. 

Insert your PixyBot's SD card into your SD card writer and click "Write". 

Once the card is written, you can insert it back into the PixyBot and boot it up. The install can take up to an hour to download and install all the necessary libraries. If it is interrupted before completion, you'll need to either complete the install yourself using the manual steps below, or flash the SD card and start over again.

### Method #2: Manual Install

With this method, you will perform the installation steps manually. This has the benefit of being able to check at each step to make sure everything has gone smoothly.

#### Download NOOBS and flash the SD card
Start by [downloading the NOOBS image](https://www.raspberrypi.org/downloads/noobs/) and copying it to the SD card. TO format the SD card, you can use utilities built into your operating system, or download a tool like [SD Formatter](https://www.sdcard.org/downloads/formatter_4/). 

#### Boot
Once the SD card is ready, insert it into the Raspberry Pi, and connect an HDMI cable, a keyboard, and a laptop. Then connect a power cable to boot the Pi. The adapter should be capable of at least 2A @ 5V.

#### Enable SSH server 
We recommend enabling SSH, so that you can connect to the Pi remotely:
1. Enter `sudo raspi-config` in a terminal window
2. Select **Interfacing Options**
3. Navigate to and select **SSH**
4. Choose **Yes**
5. Select **Ok**
6. Choose **Finish**

#### SSH to the Pi
If you have a Macbook, you can connect directly to the Pi with an ethernet cable, and then run ssh from Terminal as follows. Other operating systems require [some extra effort](https://pihw.wordpress.com/guides/direct-network-connection/).
```
$ ssh pi@raspberrypi.local
```

#### Install dependencies 
Open a terminal window and type the following command:
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

