#!/bin/sh

echo "Setting up Pixy environment..."

DIR=$(cd "$(dirname "$0")"; pwd);
echo "-- Using base directory: $DIR"

# Set keyboard to US layout
echo "-- Configuring keyboard to US"
L='us' && sudo sed -i 's/XKBLAYOUT=\"\w*"/XKBLAYOUT=\"'$L'\"/g' /etc/default/keyboard

# Clone the pixy repo
echo "-- Cloning Git repository"
cd $DIR/lib
git clone https://github.com/WomensCodingCircle/pixy.git

# Run build script
echo "-- Building PixyMon..."
cd $DIR/lib/pixy/scripts
./build_pixymon_src.sh

# Create Desktop icon for PixyMon
echo "-- Creating Desktop shortcut..."
mkdir -p /home/pi/Desktop
ln -s $DIR/lib/pixy/build/pixymon/bin/PixyMon /home/pi/Desktop/PixyMon

# Add permission to use usb
echo "-- Configuring USB..."
cd ../src/host/linux/
sudo cp $DIR/lib/pixy/src/host/linux/pixy.rules /etc/udev/rules.d/

# Install Python libs 
echo "-- Installing Python libraries..."
cd $DIR/lib/pixy/scripts
./build_robot_python_demo.sh

# Copy Python libs to src dir
echo "-- Building Python libraries..."
BUILD_DIR=$DIR/lib/pixy/build/robot_in_python
TARGET_DIR=$DIR/src/python/pixy
mkdir -p $TARGET_DIR
touch $TARGET_DIR/__init__.py
cp $BUILD_DIR/pixy.py $TARGET_DIR
cp $BUILD_DIR/_pixy.so $TARGET_DIR

# Install PySerial for communication with Teensy
echo "-- Installing PySerial..."
cd $DIR/lib
wget https://github.com/pyserial/pyserial/releases/download/v3.2.1/pyserial-3.2.1.tar.gz
tar xvfz pyserial-3.2.1.tar.gz
cd pyserial-3.2.1
sudo python setup.py install

echo "PixyBattle installation complete. Ready to fight!"

