//------------------------------------------
// PixyBattle2.ino
//
// Test program for IR hardware.  
// Sends and receives IR signals in loopback mode
// using Rc5 protocol.  Sends Rx results to Serial 
// port for monitoring.
// 
// Connection:
// Rx: pin 4 to output of TSAL6200  
// Tx: pin 5 through 120KOhm resitor to TSOP34338SS1F 
// Team select : pin 6 (high/ low)
// Left-Red : pin 15
// Left-Green : pin 16
// Left-Blue : pin 17
// Right-Red : pin 18
// Right-Green : pin 19
// Right-Blue : pin 20
//
// Receiving is done through IRremote library
// Transmit is done through IntervalTimer interrupt
//   to pulse at ~38KHz.  Protocol mark/space is 
//   done through individual functions (copied heavily
//   from IRremote library).  
//  
// For use with Pixy_IR1d (Rev20170329)
//
// VERSIONS
// 20170419: 1st release to teams
//------------------------------------------

#include "IRremote.h"

//ID 
#define  PIXY_RED		1
#define  PIXY_BLUE		0
#define  TX_MSG_RED 	204		//0b11001100
#define  TX_MSG_BLUE 	170		//0b10101010
#define  TX_MSG_LENGTH	10

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

	if (bTeam == PIXY_RED) {     
		Serial.println("TEAM: RED");
		}
	else {        
		Serial.println("TEAM: BLUE");
	}

	//TX pin
	pinMode(TX_PIN, OUTPUT);  
	digitalWrite(TX_PIN,  false);
	myTimer.begin(sendIR, 13);  // 14-->35.71KHz  13-->38.46KHz

  	// Start the receiver
  	irrecv.enableIRIn(); 
}


void setDefaultLeds (void) 
{
	if (bTeam == PIXY_RED) {     
		setColors(ENUM_COLOR_LED::red);
		}
	else {        
		setColors(ENUM_COLOR_LED::blue);
	}
}


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

void blinkLeds (void) 
{
	int del = 50;

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


void loop() {

  	//Transmit  
	if(Serial.available() > 0)
	{
    	strRequest = Serial.readStringUntil('\n');
    	strRequest.trim();

    	if (strRequest == "FIRE") 
    	{
		  	digitalWrite(ONBOARD_LED_PIN, HIGH);	  		
			setColors(ENUM_COLOR_LED::green);
			
			if (bTeam == PIXY_RED) {     
	  			sendRC5(TX_MSG_RED, TX_MSG_LENGTH);
			}
			else {        
			 	sendRC5(TX_MSG_BLUE, TX_MSG_LENGTH);
			}

		  	delay(200);	  		
		  	
		  	digitalWrite(ONBOARD_LED_PIN, LOW);
		  	setDefaultLeds();	  		
		} 	
	}

  	//Receive
  	if (irrecv.decode(&results)) {    
    	irrecv.resume(); // Receive the next value 
    	//Serial.println(results.value);

    	//check if hit
    	if ( (bTeam == PIXY_RED) && (results.value == TX_MSG_BLUE) ) {bHit = true;}
		if ( (bTeam == PIXY_BLUE) && (results.value == TX_MSG_RED) ) {bHit = true;}
		//if  ( (results.value == TX_MSG_RED) || (results.value == TX_MSG_BLUE) ) {bHit = true;}		

    	if (bHit)
    	{
    		bHit = false;
    		Serial.println("HIT");

    		blinkLeds();
			setDefaultLeds();
		}
  	}  
}
