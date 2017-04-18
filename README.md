# Pixy Battle

## Raspberry Pi Setup
As a one-time set up step, you can set up your Raspberry Pi from scratch.

### Download NOOBS and flash the SD card
You can use utilities built into your operating system, or download a tool like SD Formatter. 

### Boot
Once the SD card is ready, insert it into the Raspberry Pi, and connect an HDMI cable, a keyboard, and a laptop. Then connect a power cable to boot the Pi. The adapter should be capable of at least 2A @ 5V.

### Enable SSH server 
We recommend enabling SSH, so that you can connect to the Pi remotely:
1. Enter `sudo raspi-config` in a terminal window
2. Select **Interfacing Options**
3. Navigate to and select **SSH**
4. Choose **Yes**
5. Select **Ok**
6. Choose **Finish**

### SSH to the Pi
If you have a Macbook, you can connect directly to the Pi with an ethernet cable, and then run ssh from Terminal as follows. Other operating systems require [some extra effort](https://pihw.wordpress.com/guides/direct-network-connection/).
```
ssh pi@raspberrypi.local
```

### Install dependencies 
Open a terminal window and type the following command:
```
sudo apt-get install git libusb-1.0-0-dev \
    qt4-dev-tools g++ libboost-all-dev cmake swig
```

### Clone git repositories
```
git clone https://github.com/WomensCodingCircle/pixy.git
```




