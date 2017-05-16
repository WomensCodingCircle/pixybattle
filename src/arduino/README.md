This is the directory for Arduino code for the hardware.

Pixy Robots
-----------
Written by: Cameron Loper, May 2017
Description: IR receiver and transmitters PCB.  
Hardware Description: Teensy 3.2. See comments in code file.
Compilation: CPU speed = "96 MHz (overclocked)".  Optimate = "Fast".
Libraries: 
	"IRremote.h".  See: https://www.pjrc.com/teensy/td_libs_IRremote.html
	This library can be installed via the Arduino IDE (Sketch > Include Library > Manage Libraries)


Targets
-------
Written by: Cameron Loper, May 2017
Description: IR receiver and LEDs for targets of the PixyBattle game.
Hardware Description: Teensy 3.2. See comments in code file.
Compilation: CPU speed = "96 MHz (overclocked)".  Optimate = "Fast".
Libraries: 
	"IRremote2.h".  See: https://www.pjrc.com/teensy/td_libs_IRremote.html
	This library needs to be manually added to the libraries. 
	1) Extract "IRremote2.zip" to  Path\to\your\teensy\libraries 
		For example: C:\Program Files (x86)\Arduino\hardware\teensy\avr\libraries
	2) Now confirm that the library is installed in the Arduino IDE 
		You should see "IRremote2 in the list of available libraries (Sketch > Include Library)



