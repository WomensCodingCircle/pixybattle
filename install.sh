#!/bin/sh

echo "Setting up Pixy environment..."

DIR=$(cd "$(dirname "$0")"; pwd);
echo "Base directory: $DIR"

# Clone the pixy repo
cd $DIR/lib
git clone https://github.com/WomensCodingCircle/pixy.git

# Run build script
echo "Building PixyMon..."
cd $DIR/lib/pixy/scripts
./build_pixymon_src.sh

# Add permission to use usb
echo "Configuring USB..."
cd ../src/host/linux/
sudo cp $DIR/lib/pixy/src/host/linux/pixy.rules /etc/udev/rules.d/

# Install Python libs 
echo "Installing Python libraries..."
cd $DIR/lib/pixy/scripts
./build_robot_python_demo.sh

# Create Desktop icon for PixyMon
ln -s $DIR/lib/pixy/build/pixymon/bin/PixyMon /home/pi/Desktop/PixyMon

# Copy Python libs to src dir
BUILD_DIR=$DIR/lib/pixy/build/robot_in_python
TARGET_DIR=$DIR/src/python/pixy
mkdir $TARGET_DIR
touch $TARGET_DIR/__init__.py
cp $BUILD_DIR/pixy.py $TARGET_DIR
cp $BUILD_DIR/_pixy.so $TARGET_DIR

