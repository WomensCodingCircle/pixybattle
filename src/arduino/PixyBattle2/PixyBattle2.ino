//------------------------------------------
// PixyBattle2.ino
//
// Program to run Pixy Bots.
// Upon command fires IR burst and blinks LEDs.
// If hit then prevents firing for time period and blinks LEDs. 
// Receives IR signals using RC5 protocol.
// Receiving is done through IRremote library.
// Transmit is done through IntervalTimer interrupt
//   to pulse at ~38KHz.  Protocol mark/space is 
//   done through individual functions (copied heavily
//   from IRremote library).   
// Communicates over serial port for control.
// 
// Connection:
// Rx: pin 4 to output of TSAL6200 (IR receiver)   
// Tx: pin 5 through 120KOhm resitor to TSOP34338SS1F (IR transmitter) #not implemented#
// Team select : pin 6 (high/ low)
// Left-Red : pin 15
// Left-Green : pin 16
// Left-Blue : pin 17
// Right-Red : pin 18
// Right-Green : pin 19
// Right-Blue : pin 20
//
// For use with Pixy_IR1d (Rev20170329) PCB
//
// VERSIONS
// 20170419: 1st release to teams
// 20170515: Used for Pixy meeting on 5/17/2017.  
//		Change such that cannot fire at greater rate than 1 second
//      Added fire lockout if get hit
// 20170522: 1st release to all teams after modification of Tx resistor to lower power
//		When firing send out ~500 ms of pulses (10 pulses + 10 delays of 25ms)
//		Added a HELP command to diplay all options
// 20170524: Cleaned up documentation
//------------------------------------------
#define VERSION "20170524"
//#define DEBUG_THIS   1

#include "IRremote.h"

//ID 
#define  PIXY_RED		1
#define  PIXY_BLUE		0
#define  TX_MSG_RED 	204		//0b11001100
#define  TX_MSG_BLUE 	170		//0b10101010
#define  TX_MSG_LENGTH	10

//Timing
#define  LOOP_DELAY					50		//[ms]
#define  FIRING_COOL_DOWN_COUNT 	10		//[LOOP_DELAYs]  Cooldown when firing
#define  FIRE_LOCK_OUT 				200		//[LOOP_DELAYs]  Lockout when hit by opponent		

//Colors
enum ENUM_COLOR_LED {blue, red, green, yellow, cyan, magenta, white, black};

//Hardware
int TX_PIN 				= 5;
int RX_PIN 				= 4;
int TEAM_SELECTOR_INPUT = 6;
int ONBOARD_LED_PIN 	= 13;
int LED_LEFT_RED 		= 15;
int LED_LEFT_GREEN 		= 16;
int LED_LEFT_BLUE 		= 17;
int LED_RIGHT_RED 		= 18;
int LED_RIGHT_GREEN 	= 19;
int LED_RIGHT_BLUE 		= 20;

//Pixy
String strRequest;
bool bTeam;
int FiringCoolDownCounter;
int FiringLockoutCounter;

//Receive
IRrecv irrecv(RX_PIN);
decode_results results;
bool bHit = false;

//Transmit
#define RC5_T1 889  //timing for RC5 protocol
IntervalTimer myTimer; // Create an IntervalTimer object 
volatile bool IREnabled = false; // use volatile for shared variables
volatile int ledState = LOW;
unsigned int TxMsg = 0;



void setup()
{
	Serial.begin(115200);

#ifdef DEBUG_THIS 
	delay(1000);	
	Serial.println("Pixy Battle (DEBUG)");
	Serial.print("Version: ");
	Serial.println(VERSION);
	Serial.println();	
#endif 

	pinMode(LED_RIGHT_RED, OUTPUT);  
	pinMode(LED_RIGHT_GREEN, OUTPUT);  
	pinMode(LED_RIGHT_BLUE, OUTPUT);  	
	pinMode(LED_LEFT_RED, OUTPUT);  
	pinMode(LED_LEFT_GREEN, OUTPUT);   
	pinMode(LED_LEFT_BLUE, OUTPUT);   
	pinMode(ONBOARD_LED_PIN, OUTPUT); 
	pinMode(TEAM_SELECTOR_INPUT, INPUT_PULLUP);

	//Select team based off of input pin 
	bTeam = digitalRead(TEAM_SELECTOR_INPUT); 

	delay(1000);

	//blink hello
	digitalWrite(ONBOARD_LED_PIN, HIGH); 
	delay(200);	
	digitalWrite(ONBOARD_LED_PIN, LOW); 	   
	blinkLeds();
	setDefaultLeds();

	//TX pin
	pinMode(TX_PIN, OUTPUT);  
	digitalWrite(TX_PIN,  false);
	myTimer.begin(sendIR, 13);  // 14-->35.71KHz  13-->38.46KHz

  	// Start the receiver
  	irrecv.enableIRIn(); 

  	FiringCoolDownCounter = 0;
  	FiringLockoutCounter = 0;
}


//Set LED based on bTeam (e.g. from switch) 
void setDefaultLeds (void) 
{
	if (bTeam == PIXY_RED) {     
		setColors(ENUM_COLOR_LED::red);
		}
	else {        
		setColors(ENUM_COLOR_LED::blue);
	}
}


//Set LED colors
void setColors (ENUM_COLOR_LED col)
{
	switch (col)
	{
		case ENUM_COLOR_LED::red:
			digitalWrite(LED_RIGHT_RED, HIGH); 
			digitalWrite(LED_RIGHT_GREEN, LOW); 
			digitalWrite(LED_RIGHT_BLUE, LOW); 
			digitalWrite(LED_LEFT_RED, HIGH); 
			digitalWrite(LED_LEFT_GREEN, LOW); 
			digitalWrite(LED_LEFT_BLUE, LOW); 
			break;
		case ENUM_COLOR_LED::green:
			digitalWrite(LED_RIGHT_RED, LOW); 
			digitalWrite(LED_RIGHT_GREEN, HIGH); 
			digitalWrite(LED_RIGHT_BLUE, LOW); 
			digitalWrite(LED_LEFT_RED, LOW); 
			digitalWrite(LED_LEFT_GREEN, HIGH); 
			digitalWrite(LED_LEFT_BLUE, LOW); 
			break;
		case ENUM_COLOR_LED::blue:
			digitalWrite(LED_RIGHT_RED, LOW); 
			digitalWrite(LED_RIGHT_GREEN, LOW); 
			digitalWrite(LED_RIGHT_BLUE, HIGH); 
			digitalWrite(LED_LEFT_RED, LOW); 
			digitalWrite(LED_LEFT_GREEN, LOW); 
			digitalWrite(LED_LEFT_BLUE, HIGH); 
			break;
		case ENUM_COLOR_LED::cyan:
			digitalWrite(LED_RIGHT_RED, LOW); 
			digitalWrite(LED_RIGHT_GREEN, HIGH); 
			digitalWrite(LED_RIGHT_BLUE, HIGH); 
			digitalWrite(LED_LEFT_RED, LOW); 
			digitalWrite(LED_LEFT_GREEN, HIGH); 
			digitalWrite(LED_LEFT_BLUE, HIGH); 
			break;
		case ENUM_COLOR_LED::magenta:
			digitalWrite(LED_RIGHT_RED, HIGH); 
			digitalWrite(LED_RIGHT_GREEN, LOW); 
			digitalWrite(LED_RIGHT_BLUE, HIGH); 
			digitalWrite(LED_LEFT_RED, HIGH); 
			digitalWrite(LED_LEFT_GREEN, LOW); 
			digitalWrite(LED_LEFT_BLUE, HIGH); 
			break;
		case ENUM_COLOR_LED::yellow:
			digitalWrite(LED_RIGHT_RED, HIGH); 
			digitalWrite(LED_RIGHT_GREEN, HIGH); 
			digitalWrite(LED_RIGHT_BLUE, LOW); 
			digitalWrite(LED_LEFT_RED, HIGH); 
			digitalWrite(LED_LEFT_GREEN, HIGH); 
			digitalWrite(LED_LEFT_BLUE, LOW); 
			break;
		case ENUM_COLOR_LED::white:
			digitalWrite(LED_RIGHT_RED, HIGH); 
			digitalWrite(LED_RIGHT_GREEN, HIGH); 
			digitalWrite(LED_RIGHT_BLUE, HIGH); 
			digitalWrite(LED_LEFT_RED, HIGH); 
			digitalWrite(LED_LEFT_GREEN, HIGH); 
			digitalWrite(LED_LEFT_BLUE, HIGH); 
			break;																	
		case ENUM_COLOR_LED::black:
		default:
			digitalWrite(LED_RIGHT_RED, LOW); 
			digitalWrite(LED_RIGHT_GREEN, LOW); 
			digitalWrite(LED_RIGHT_BLUE, LOW); 
			digitalWrite(LED_LEFT_RED, LOW); 
			digitalWrite(LED_LEFT_GREEN, LOW); 
			digitalWrite(LED_LEFT_BLUE, LOW); 
			break;
	}
}


//Cycle through LED colors
void blinkLeds (void) 
{
	int del = 50;

#ifdef DEBUG_THIS 
	del = 1000;
#endif

	setColors(ENUM_COLOR_LED::red);
	delay(del);

	setColors(ENUM_COLOR_LED::green);
	delay(del);

	setColors(ENUM_COLOR_LED::blue);
	delay(del);

	setColors(ENUM_COLOR_LED::yellow);
	delay(del);

	setColors(ENUM_COLOR_LED::cyan);
	delay(del);

	setColors(ENUM_COLOR_LED::magenta);
	delay(del);

	setColors(ENUM_COLOR_LED::white);
	delay(del);

	setColors(ENUM_COLOR_LED::black);
	delay(del);										
}


//IntervalTimer function to pulse Tx 
void sendIR(void) {    

	if (IREnabled) { 
		ledState = !ledState;        
		digitalWrite(TX_PIN, ledState); 
	}
	else {
		ledState = false;    
		digitalWrite(TX_PIN, ledState); 
	}
}


//Mark
void  markIR (unsigned int time)
{
	IREnabled = true; // Enable output
	if (time > 0) custom_delay_usec(time);
}


//Space
void  spaceIR (unsigned int time)
{
	IREnabled = false; // Disable  output
	if (time > 0) custom_delay_usec(time);
}


//Transmit a message using RC5
void sendRC5 (unsigned long data,  int nbits)
{
	//start timer
	ledState = true;
	myTimer.begin(sendIR, 14);  // 14-->35.71KHz  13-->38.46KHz

	// Start sequence
	markIR(RC5_T1);
	spaceIR(RC5_T1);
	markIR(RC5_T1);

  	// Data sequence
  	for (unsigned long  mask = 1UL << (nbits - 1);  mask;  mask >>= 1) {
	    if (data & mask) {
	      	spaceIR(RC5_T1); // 1 is space, then mark
	      	markIR(RC5_T1);
	    } 
	    else {
	     	 markIR(RC5_T1);
	      	spaceIR(RC5_T1);
	    }   
    }

	//stop timer
	myTimer.end();

	// Always end with the LED off
	digitalWrite(TX_PIN, LOW); 
	ledState = false;
}

// Custom delay function that circumvents Arduino's delayMicroseconds limit
void custom_delay_usec(unsigned long uSecs) {
	if (uSecs > 4) {
		unsigned long start = micros();
		unsigned long endMicros = start + uSecs - 4;
		if (endMicros < start) { // Check if overflow
	  		while ( micros() > start ) {} // wait until overflow
		}
		while ( micros() < endMicros ) {} // normal wait
	} 
}



//Main loop
void loop() {
	
	delay (LOOP_DELAY);

  	//Transmit  
	if(Serial.available() > 0)
	{
    	strRequest = Serial.readStringUntil('\n');
    	strRequest.trim();

    	if ((strRequest == "FIRE") && (FiringCoolDownCounter == 0) && (FiringLockoutCounter == 0)) 
    	{
		  	digitalWrite(ONBOARD_LED_PIN, HIGH);	  		
			setColors(ENUM_COLOR_LED::green);
			
			if (bTeam == PIXY_RED) { 
				//repeat for ~500ms (@ ~25ms per message)    
				for (int i = 0; i<10; i++)
	  			{
	  				sendRC5(TX_MSG_RED, TX_MSG_LENGTH);
	  				delay(25);
	  			}
			}
			else {        
				//repeat for ~500ms (@ ~25ms per message)    
				for (int i = 0; i<10; i++)
	  			{
	  				sendRC5(TX_MSG_BLUE, TX_MSG_LENGTH);
	  				delay(25);
	  			}	  			 				  				  			
			} 		
		
			//reset colors  	
		 	digitalWrite(ONBOARD_LED_PIN, LOW);
		   	setDefaultLeds();

			FiringCoolDownCounter = FIRING_COOL_DOWN_COUNT;
		} 	
		else if (strRequest == "TEAM")
		{
			if (bTeam == PIXY_RED) {     
				Serial.println("RED");
				}
			else {        
				Serial.println("BLUE");
			}			
		}
    	else if (strRequest == "HELP")
    	{
    		Serial.println("Pixy Battle Bot");
    		Serial.println("TX:");
    		Serial.println("  <RED/BLUE>     --> upon receiving 'TEAM?' command");
    		Serial.println("RX:");
    		Serial.println("  FIRE           --> to fire IR pulses");
    		Serial.println("  TEAM           --> to display team color");
    		Serial.println("  HELP           --> list of commands");
    		Serial.println("  VERSION        --> code version");
    		Serial.println();    	 	    	
		}	
    	else if (strRequest == "VERSION")
    	{
    		Serial.println(VERSION);
    	}			
	}

	//Fire cooldown
	if (FiringCoolDownCounter > 0) {
#ifdef DEBUG_THIS 
		//Serial.println(FiringCoolDownCounter);
#endif	
		FiringCoolDownCounter--;
	}

	//Fire lockout
	if (FiringLockoutCounter > 0) {
#ifdef DEBUG_THIS 
		//Serial.println(FiringLockoutCounter);
#endif		
		FiringLockoutCounter--;

		//blink firing leds 
		if (FiringLockoutCounter % 2) {
			digitalWrite(ONBOARD_LED_PIN, HIGH);
			setDefaultLeds();	
		}
		else {
			digitalWrite(ONBOARD_LED_PIN, LOW);
			setColors(ENUM_COLOR_LED::black);				
		}

		//reset leds on exit
		if (FiringLockoutCounter == 0) {
			digitalWrite(ONBOARD_LED_PIN, LOW);
		  	setDefaultLeds();	  		
		}
	}


  	//Receive
  	if (irrecv.decode(&results)) {    
    	irrecv.resume(); // Receive the next value 

#ifdef DEBUG_THIS     	
    	//Serial.println(results.value);
#endif

    	//check if hit
    	if (FiringLockoutCounter == 0) 
    	{
	    	if ( (bTeam == PIXY_RED) && (results.value == TX_MSG_BLUE) ) {bHit = true;}
			if ( (bTeam == PIXY_BLUE) && (results.value == TX_MSG_RED) ) {bHit = true;}	

#ifdef DEBUG_THIS 
			if ( (results.value == TX_MSG_BLUE) || (results.value == TX_MSG_RED) ) {bHit = true;}	
#endif
	    	if (bHit)
	    	{
	    		bHit = false;
				
				FiringLockoutCounter = FIRE_LOCK_OUT;
				 
				digitalWrite(ONBOARD_LED_PIN, LOW);			   					
				setColors(ENUM_COLOR_LED::black);
	    		
	    		Serial.println("HIT");
			}
	  	}
	}  
}
