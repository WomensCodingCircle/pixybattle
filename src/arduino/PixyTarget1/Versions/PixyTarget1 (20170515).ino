//------------------------------------------
// PixyTarget1.ino
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
//------------------------------------------
#define VERSION "20170514"

#include "IRremote2.h"

//timing
#define HR_CLOCK_PERIOD_IN_USEC     500000  //= 500ms [uSec] 
#define USEC_PER_SEC                1000000
//#define HR_CLOCKS_PER_SEC		    USEC_PER_SEC / HR_CLOCK_PERIOD_IN_USEC 

#define CLOCK_TIMEOUT_TRIGGER		1 * USEC_PER_SEC;	//[sec]	
#define CLOCK_TIMEOUT_GO_NEUTRAL	5 * USEC_PER_SEC;	//[sec]
//#define CLOCK_TIMEOUT_GAME_OVER		10 * USEC_PER_SEC;	//[sec]


IntervalTimer hrClock;                          //high resolution timer to measure LED time on    
volatile unsigned long hrClockCount = 0;        // use volatile for shared variables

//ID 
#define  PIXY_RED		1
#define  PIXY_BLUE		0
#define  TX_MSG_RED 	204		//0b11001100
#define  TX_MSG_BLUE 	170		//0b10101010
#define  TX_MSG_LENGTH	10
#define  TX_SEL_LEFT	LOW
#define  TX_SEL_RIGHT	HIGH

//Hardware
int IR_RX_L_PIN			= 4;
int TX_PIN 				= 5;
int IR_RX_R_PIN			= 6;
int TX_SEL_PIN 			= 7;
int ONBOARD_LED_PIN 	= 13;
int LED_LEFT_RED 		= 18;
int LED_LEFT_GREEN 		= 19;
int LED_LEFT_BLUE 		= 20;
int LED_RIGHT_RED 		= 15;
int LED_RIGHT_GREEN 	= 16;
int LED_RIGHT_BLUE 		= 17;

//Colors
enum ENUM_COLOR_LED {blue, red, green, yellow, cyan, magenta, white, black};

//Sides
enum ENUM_SIDE {LEFT, RIGHT};

//Hit state 
enum ENUM_TRIGGERED_STATE {NOT_TRIGGERED, TRIGGERED};
ENUM_TRIGGERED_STATE triggerState[2] = {ENUM_TRIGGERED_STATE::NOT_TRIGGERED, ENUM_TRIGGERED_STATE::NOT_TRIGGERED};

enum ENUM_HIT_STATE {HIT_NONE, HIT_BLUE, HIT_RED};
ENUM_HIT_STATE hitState[2] = {ENUM_HIT_STATE::HIT_NONE, ENUM_HIT_STATE::HIT_NONE}; 	//tracks current state (R/B)
ENUM_HIT_STATE newHit[2] = {ENUM_HIT_STATE::HIT_NONE, ENUM_HIT_STATE::HIT_NONE};	//tracks new hits

//hit total [1st hit, 2nd hit]
// int hitTotalLeftRed[2] = {0,0};
// int hitTotalLeftBlue[2] = {0,0};
// int hitTotalRightRed[2] = {0,0};
// int hitTotalRightBlue[2] = {0,0};

//Time trackers
unsigned long triggerCount[2];			//timeout  -> side is triggered
unsigned long revertToNeutralCount[2];	//timeout  -> go back to neutral state from hit state
//long gameCompleteCount;			//timeout  -> game is over, go back to idle 

//Pixy
String strRequest;
enum ENUM_STATE {NONE, IDLE, NEUTRAL, FIRST_HIT, SECOND_HIT};
ENUM_STATE theState[2] = {ENUM_STATE::NONE, ENUM_STATE::NONE};		//[LEFT, RIGHT]


//Receive
IRrecv irrecv(IR_RX_R_PIN);
decode_results results;


//Controller messaging
String    strRxController;		//Message from controller                                


void setup()
{
	Serial.begin(115200);

	//Pin modes
	pinMode(LED_RIGHT_RED, OUTPUT);  
	pinMode(LED_RIGHT_GREEN, OUTPUT);  
	pinMode(LED_RIGHT_BLUE, OUTPUT);  	
	pinMode(LED_LEFT_RED, OUTPUT);  
	pinMode(LED_LEFT_GREEN, OUTPUT);   
	pinMode(LED_LEFT_BLUE, OUTPUT);   
	pinMode(ONBOARD_LED_PIN, OUTPUT); 
	
	pinMode(TX_SEL_PIN, OUTPUT);
	digitalWrite(TX_SEL_PIN, TX_SEL_LEFT); 

	pinMode(IR_RX_L_PIN, INPUT_PULLUP); 
	pinMode(IR_RX_R_PIN, INPUT_PULLUP);

	//blink hello
	digitalWrite(ONBOARD_LED_PIN, HIGH); 
	delay(200);	
	digitalWrite(ONBOARD_LED_PIN, LOW); 	   
	blinkLeds();

	//TX pin
	pinMode(TX_PIN, OUTPUT);  
	digitalWrite(TX_PIN,  false);

  	// Start the receiver
  	//irrecv.enableIRIn(); 

    //reset and start clock 
	hrClockCount = 0;
	hrClock.priority(128); 

	//State
	StateEngine_SetNewState(ENUM_STATE::IDLE, ENUM_SIDE::LEFT);
	StateEngine_SetNewState(ENUM_STATE::IDLE, ENUM_SIDE::RIGHT);	
}


void hrTimerCount()
{
    hrClockCount+= HR_CLOCK_PERIOD_IN_USEC;   //ignore oveflow (~2.4 hours @500ms period)  
}




//Overload for SetColors to set color by a hit state 
void SetColors (ENUM_HIT_STATE team, ENUM_SIDE side)
{
	if (team == ENUM_HIT_STATE::HIT_NONE) { SetColors(ENUM_COLOR_LED::green, side); }
	else if (team == ENUM_HIT_STATE::HIT_BLUE) { SetColors(ENUM_COLOR_LED::blue, side); }
	else if (team == ENUM_HIT_STATE::HIT_RED) { SetColors(ENUM_COLOR_LED::red, side); }
}


//Set a color for a side
void SetColors (ENUM_COLOR_LED col, ENUM_SIDE side)
{
	if (side == ENUM_SIDE::LEFT) { 
		switch (col)
		{
			case ENUM_COLOR_LED::red:
				digitalWrite(LED_LEFT_RED, HIGH); 
				digitalWrite(LED_LEFT_GREEN, LOW); 
				digitalWrite(LED_LEFT_BLUE, LOW); 
				break;
			case ENUM_COLOR_LED::green:
				digitalWrite(LED_LEFT_RED, LOW); 
				digitalWrite(LED_LEFT_GREEN, HIGH); 
				digitalWrite(LED_LEFT_BLUE, LOW); 
				break;
			case ENUM_COLOR_LED::blue:
				digitalWrite(LED_LEFT_RED, LOW); 
				digitalWrite(LED_LEFT_GREEN, LOW); 
				digitalWrite(LED_LEFT_BLUE, HIGH); 
				break;
			case ENUM_COLOR_LED::cyan:
				digitalWrite(LED_LEFT_RED, LOW); 
				digitalWrite(LED_LEFT_GREEN, HIGH); 
				digitalWrite(LED_LEFT_BLUE, HIGH); 
				break;
			case ENUM_COLOR_LED::magenta:
				digitalWrite(LED_LEFT_RED, HIGH); 
				digitalWrite(LED_LEFT_GREEN, LOW); 
				digitalWrite(LED_LEFT_BLUE, HIGH); 
				break;
			case ENUM_COLOR_LED::yellow:
				digitalWrite(LED_LEFT_RED, HIGH); 
				digitalWrite(LED_LEFT_GREEN, HIGH); 
				digitalWrite(LED_LEFT_BLUE, LOW); 
				break;
			case ENUM_COLOR_LED::white:
				digitalWrite(LED_LEFT_RED, HIGH); 
				digitalWrite(LED_LEFT_GREEN, HIGH); 
				digitalWrite(LED_LEFT_BLUE, HIGH); 
				break;																	
			case ENUM_COLOR_LED::black:
			default:
				digitalWrite(LED_LEFT_RED, LOW); 
				digitalWrite(LED_LEFT_GREEN, LOW); 
				digitalWrite(LED_LEFT_BLUE, LOW); 
				break;
		}
	}
	else if (side == ENUM_SIDE::RIGHT) 
	{
		switch (col)
		{
			case ENUM_COLOR_LED::red:
				digitalWrite(LED_RIGHT_RED, HIGH); 
				digitalWrite(LED_RIGHT_GREEN, LOW); 
				digitalWrite(LED_RIGHT_BLUE, LOW); 
				break;
			case ENUM_COLOR_LED::green:
				digitalWrite(LED_RIGHT_RED, LOW); 
				digitalWrite(LED_RIGHT_GREEN, HIGH); 
				digitalWrite(LED_RIGHT_BLUE, LOW); 
				break;
			case ENUM_COLOR_LED::blue:
				digitalWrite(LED_RIGHT_RED, LOW); 
				digitalWrite(LED_RIGHT_GREEN, LOW); 
				digitalWrite(LED_RIGHT_BLUE, HIGH); 
				break;
			case ENUM_COLOR_LED::cyan:
				digitalWrite(LED_RIGHT_RED, LOW); 
				digitalWrite(LED_RIGHT_GREEN, HIGH); 
				digitalWrite(LED_RIGHT_BLUE, HIGH); 
				break;
			case ENUM_COLOR_LED::magenta:
				digitalWrite(LED_RIGHT_RED, HIGH); 
				digitalWrite(LED_RIGHT_GREEN, LOW); 
				digitalWrite(LED_RIGHT_BLUE, HIGH); 
				break;
			case ENUM_COLOR_LED::yellow:
				digitalWrite(LED_RIGHT_RED, HIGH); 
				digitalWrite(LED_RIGHT_GREEN, HIGH); 
				digitalWrite(LED_RIGHT_BLUE, LOW); 
				break;
			case ENUM_COLOR_LED::white:
				digitalWrite(LED_RIGHT_RED, HIGH); 
				digitalWrite(LED_RIGHT_GREEN, HIGH); 
				digitalWrite(LED_RIGHT_BLUE, HIGH); 
				break;																	
			case ENUM_COLOR_LED::black:
			default:
				digitalWrite(LED_RIGHT_RED, LOW); 
				digitalWrite(LED_RIGHT_GREEN, LOW); 
				digitalWrite(LED_RIGHT_BLUE, LOW); 
				break;
		}		
	} 	
}


void blinkLeds (void) 
{
	int del = 100;

	SetColors(ENUM_COLOR_LED::red, ENUM_SIDE::LEFT);
	SetColors(ENUM_COLOR_LED::red, ENUM_SIDE::RIGHT);
	delay(del);

	SetColors(ENUM_COLOR_LED::green, ENUM_SIDE::LEFT);
	SetColors(ENUM_COLOR_LED::green, ENUM_SIDE::RIGHT);	
	delay(del);

	SetColors(ENUM_COLOR_LED::blue, ENUM_SIDE::LEFT);
	SetColors(ENUM_COLOR_LED::blue, ENUM_SIDE::RIGHT);
	delay(del);

	SetColors(ENUM_COLOR_LED::yellow, ENUM_SIDE::LEFT);
	SetColors(ENUM_COLOR_LED::yellow, ENUM_SIDE::RIGHT);
	delay(del);

	SetColors(ENUM_COLOR_LED::cyan, ENUM_SIDE::LEFT);
	SetColors(ENUM_COLOR_LED::cyan, ENUM_SIDE::RIGHT);
	delay(del);

	SetColors(ENUM_COLOR_LED::magenta, ENUM_SIDE::LEFT);
	SetColors(ENUM_COLOR_LED::magenta, ENUM_SIDE::RIGHT);
	delay(del);

	SetColors(ENUM_COLOR_LED::black, ENUM_SIDE::LEFT);
	SetColors(ENUM_COLOR_LED::black, ENUM_SIDE::RIGHT);
	delay(del);										
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



//===STATE MACHINE===

//State leaving andentry code
void StateEngine_SetNewState (ENUM_STATE newState, ENUM_SIDE side)
{
	switch (newState) {

		case ENUM_STATE::IDLE:
			SetColors(ENUM_COLOR_LED::green,side);	
			hrClock.end();
			break; 

		case ENUM_STATE::NEUTRAL:
			if (theState[side] == ENUM_STATE::IDLE) {	
				hrClockCount = 0;	//reset clock
  				hrClock.begin(hrTimerCount, HR_CLOCK_PERIOD_IN_USEC);  				
				//TODO: Reset hit counters
			}

			hitState[side] = ENUM_HIT_STATE::HIT_NONE;	
			SetColors(hitState[side],side);								//Green
			triggerState[side] = ENUM_TRIGGERED_STATE::NOT_TRIGGERED; 	//not triggerd yet
			triggerCount[side] = hrClockCount + CLOCK_TIMEOUT_TRIGGER; 	//trigger timout

			revertToNeutralCount[side] = hrClockCount + CLOCK_TIMEOUT_GO_NEUTRAL;

			//Send serial message that going to neutral state
			Serial.print("NEUTRAL,");
			if (side == ENUM_SIDE::LEFT) { Serial.println("LEFT"); }
			else if (side == ENUM_SIDE::RIGHT) { Serial.println("RIGHT"); }

			break; 

		case ENUM_STATE::FIRST_HIT:
			hitState[side] = newHit[side];	
			SetColors(hitState[side],side);								//Blue or red		 			
			triggerState[side] = ENUM_TRIGGERED_STATE::NOT_TRIGGERED; 	//not triggerd yet
			triggerCount[side] = hrClockCount + CLOCK_TIMEOUT_TRIGGER; 	//trigger timout

			SendSerialMessage_Hit(newHit[side], side);			
			break; 

		case ENUM_STATE::SECOND_HIT:
			hitState[side] = newHit[side];	
			SetColors(hitState[side],side);								//Blue or red		 			
			triggerState[side] = ENUM_TRIGGERED_STATE::NOT_TRIGGERED; 	//not triggerd yet
			triggerCount[side] = hrClockCount + CLOCK_TIMEOUT_TRIGGER; 	//trigger timout

			SendSerialMessage_Hit(newHit[side], side);			
			break; 

		default:
			break;
	}

	theState[side] = newState;	
}



//State normal and leaving code 
void StateEngine_Parse (ENUM_SIDE side)
{
	Serial.print("state = ");
	Serial.print(theState[side]);
	Serial.print(", side = ");
	Serial.println(side);
		
	//State and/or Entering a state
	switch (theState[side]) {

		case ENUM_STATE::IDLE:
			//TODO: Blinking of red/blue
			break; 

		case ENUM_STATE::NEUTRAL:
			//check for any hit while triggered
			if 	((triggerState[side] == ENUM_TRIGGERED_STATE::TRIGGERED) && 
				 (newHit[side])) {	
				StateEngine_SetNewState(ENUM_STATE::FIRST_HIT, side);				
			}
	
			break; 

		case ENUM_STATE::FIRST_HIT:
			//while triggered
			if 	(triggerState[side] == ENUM_TRIGGERED_STATE::TRIGGERED) {

				//check for a new hit different than current color state
				if ((hitState[side] == ENUM_HIT_STATE::HIT_BLUE) &&
 					(newHit[side] ==  ENUM_HIT_STATE::HIT_RED)) {
					StateEngine_SetNewState(ENUM_STATE::SECOND_HIT, side);				
				}
				else if ((hitState[side] == ENUM_HIT_STATE::HIT_RED) &&
 					(newHit[side] ==  ENUM_HIT_STATE::HIT_BLUE)) {
					StateEngine_SetNewState(ENUM_STATE::SECOND_HIT, side);				
				}
			}		

			//check for revert to neutral timeout
			if (hrClockCount > revertToNeutralCount[side]) {
				StateEngine_SetNewState(ENUM_STATE::NEUTRAL, side);
			}  
			break; 

		case ENUM_STATE::SECOND_HIT:
			//check for revert to neutral timeout
			if (hrClockCount > revertToNeutralCount[side]) {
				StateEngine_SetNewState(ENUM_STATE::NEUTRAL, side);
			}  
			break; 		

		default:
			break;		
	}	
}


//Send hit message
//  Formate: HIT,COLOR,SIDE
//  Ex: HIT,RED,LEFT
void SendSerialMessage_Hit(ENUM_HIT_STATE team, ENUM_SIDE side)
{
	Serial.print("HIT,");

	if (team == ENUM_HIT_STATE::HIT_BLUE) { Serial.print("BLUE,"); }
	else if (team == ENUM_HIT_STATE::HIT_RED) { Serial.print("RED,"); }

	if (side == ENUM_SIDE::LEFT) { Serial.println("LEFT"); }
	else if (side == ENUM_SIDE::RIGHT) { Serial.println("RIGHT"); }
}



void loop() {

	//== CONTROLLER MESSAGE RECEIVER==
	if(Serial.available() > 0)
	{
    	strRxController = Serial.readStringUntil('\n');
    	strRxController.trim();

    	if (strRxController == "START") 
    	{
    		Serial.println("RX:START");
    		//go to neutral state
    		if (theState[ENUM_SIDE::LEFT] == ENUM_STATE::IDLE) {
    			StateEngine_SetNewState(ENUM_STATE::NEUTRAL, ENUM_SIDE::LEFT);
    			StateEngine_Parse(ENUM_SIDE::LEFT);
    		}

    		if (theState[ENUM_SIDE::RIGHT] == ENUM_STATE::IDLE) {	    		
    			StateEngine_SetNewState(ENUM_STATE::NEUTRAL, ENUM_SIDE::RIGHT);
    			StateEngine_Parse(ENUM_SIDE::RIGHT);
    		}    		
    		
    		//TODO: set gameCompleteCount based on data within message;		 				
    	}
    	if (strRxController == "STOP") 
    	{
    		Serial.println("RX:START");
    		StateEngine_SetNewState(ENUM_STATE::IDLE, ENUM_SIDE::LEFT);
			StateEngine_Parse(ENUM_SIDE::LEFT);	
    		StateEngine_SetNewState(ENUM_STATE::IDLE, ENUM_SIDE::RIGHT);
			StateEngine_Parse(ENUM_SIDE::RIGHT);
    	}
    	if (strRxController == "SCORE") 
    	{
    		//TODO: Send hit counts
    	}
    }


	//TODO: check if end of game timeout via gameCompleteCount


	//==LEFT==
	if (theState[ENUM_SIDE::LEFT] != ENUM_STATE::IDLE) {

		//check if triggered - timeout
		if (triggerState[ENUM_SIDE::LEFT] == ENUM_TRIGGERED_STATE::NOT_TRIGGERED) {
			if (hrClockCount > triggerCount[ENUM_SIDE::RIGHT]) {
				triggerState[ENUM_SIDE::LEFT] = ENUM_TRIGGERED_STATE::TRIGGERED;
			}  
		}
		

	  	//Check if hit - IR Receive 
	  	//IRrecv irrecvL(IR_RX_L_PIN);
	  	irrecv.enableIRIn(IR_RX_L_PIN); 
	  	delay(50);

	  	if (irrecv.decode(&results)) {    

	    	//check if hit
	    	if (results.value == TX_MSG_BLUE) {
				newHit[ENUM_SIDE::LEFT] = ENUM_HIT_STATE::HIT_BLUE;
			}
			else if (results.value == TX_MSG_RED) {
				newHit[ENUM_SIDE::LEFT] = ENUM_HIT_STATE::HIT_RED;
			}
			else {
				newHit[ENUM_SIDE::LEFT] = ENUM_HIT_STATE::HIT_NONE;
			}

			irrecv.resume(); // Receive the next value 		
	  	}  
	}
  	StateEngine_Parse(ENUM_SIDE::LEFT);


  	//==RIGHT==
	if (theState[ENUM_SIDE::RIGHT] != ENUM_STATE::IDLE) {

		//check if triggered - timeout
		if (triggerState[ENUM_SIDE::RIGHT] == ENUM_TRIGGERED_STATE::NOT_TRIGGERED) {
			if (hrClockCount > triggerCount[ENUM_SIDE::RIGHT]) {
				triggerState[ENUM_SIDE::RIGHT] = ENUM_TRIGGERED_STATE::TRIGGERED;
			}  
		}
	  	
	  	//Check if hit - IR Receive  
	  	//IRrecv irrecvR(IR_RX_R_PIN);
	  	irrecv.enableIRIn(IR_RX_R_PIN); 
	  	delay(50);

	  	if (irrecv.decode(&results)) {    

	    	//check if hit
	    	if (results.value == TX_MSG_BLUE) {
				newHit[ENUM_SIDE::RIGHT] = ENUM_HIT_STATE::HIT_BLUE;
	    	}
			else if (results.value == TX_MSG_RED)  {
				newHit[ENUM_SIDE::RIGHT] = ENUM_HIT_STATE::HIT_RED;
			}
			else {
				newHit[ENUM_SIDE::RIGHT] = ENUM_HIT_STATE::HIT_NONE;			
			}

	  		irrecv.resume(); // Receive the next value 
	  	}    	
  	}
  	StateEngine_Parse(ENUM_SIDE::LEFT);


}
