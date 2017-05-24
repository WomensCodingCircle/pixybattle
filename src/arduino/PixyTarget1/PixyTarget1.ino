//------------------------------------------
// PixyTarget1.ino
//
// Program to run PixyBattle target.
// Colors changed based on state engine (Green = no hit, Blue/Red = hit). 
// Receives IR signals using RC5 protocol.
// Receiving is done sequentially per side (e.g. left -> right -> left ...)
// Receiving is done through IRremote library. 
// Communicates over serial port for monitoring.
// 
// Connection:
// Rx: pin 4 to output of TSAL6200 (IR receiver)   
// Rx - side selector: pin 6  
// Tx: pin 5 through 120KOhm resitor to TSOP34338SS1F (IR transmitter) #not implemented#
// Tx - side selector: pin 7  #not implemented#
// 
// Left-Red : pin 18
// Left-Green : pin 19
// Left-Blue : pin 20
// Right-Red : pin 15
// Right-Green : pin 16
// Right-Blue : pin 27
//
// Monitor communications:
// TX:
//   NEUTRAL,<LEFT/RIGHT>  						--> when going to default (green)
//   HIT,<RED/BLUE,<LEFT/RIGHT>  				--> when getting 1st or 2nd hit 
//   <RED/BLUE>:#1st hits,#2nd hits,#final 		--> upon receiving "SCORE" command
// RX:
//	 START  									--> starts the game
//   STOP 										--> stops the game 
//   SCORE 										--> sends total hit counts
//	 TEST_RED  									--> sets both sides to red
//   TEST_BLUE  								--> sets both sides to blue
//   TEST_GREEN  								--> sets both sides to green
//   TEST_RED_BLUE		 						--> sets one side to red, one side to blue
//   TEST_BLUE_HIT								--> Simulate a blue hit to both sides	
//   TEST_RED_HIT								--> Simulate a red hit to both sides 	
//   RESET          							--> reset target (ready to receive 'START')
//   HELP										--> list of commands
//   VERSION  									--> code version 
//  
// For use with PixyTarget1b (Rev20174013) PCB
// 
// VERSIONS
// 20170515: Preliminary version
// 20170522: 1st release to all teams
//		Added RX_WAIT_TIME, set to 100 ms
// 20170524: Cleaned up documentation
//------------------------------------------
#define VERSION "20170524"
//#define DEBUG_THIS   1

#include "IRremote2.h"

//timing
#define HR_CLOCK_PERIOD_IN_USEC     500000  //= 500ms [uSec] 
#define USEC_PER_SEC                1000000

#define CLOCK_TIMEOUT_TRIGGER		1000000	//[sec]	
#define CLOCK_TIMEOUT_GO_NEUTRAL	20 * USEC_PER_SEC;	//[sec]
#define CLOCK_TIMEOUT_BLINK			500000	//[sec]

#define RX_WAIT_TIME				100		//[ms]  delay time to receive a signal per side

IntervalTimer hrClock;                      //high resolution timer to measure LED time on    
volatile unsigned long hrClockCount = 0;    //use volatile for shared variables

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
bool bBlinkState[2];

//Sides
enum ENUM_SIDE {LEFT, RIGHT};

//Hit state 
enum ENUM_TRIGGERED_STATE {NOT_TRIGGERED, TRIGGERED};
ENUM_TRIGGERED_STATE triggerState[2] = {ENUM_TRIGGERED_STATE::NOT_TRIGGERED, ENUM_TRIGGERED_STATE::NOT_TRIGGERED};

enum ENUM_HIT_STATE {HIT_NONE, HIT_BLUE, HIT_RED};
ENUM_HIT_STATE hitState[2] = {ENUM_HIT_STATE::HIT_NONE, ENUM_HIT_STATE::HIT_NONE}; 	//tracks current state (R/B)
ENUM_HIT_STATE newHit[2] = {ENUM_HIT_STATE::HIT_NONE, ENUM_HIT_STATE::HIT_NONE};	//tracks new hits

int hitTotalBlueFirst = 0;
int hitTotalRedFirst = 0;
int hitTotalBlueSecond = 0;
int hitTotalRedSecond = 0;

//Time trackers
unsigned long triggerCount[2];			//timeout  -> side is triggered
unsigned long revertToNeutralCount[2];	//timeout  -> go back to neutral state from hit state
unsigned long blinkCount[2];			//Used to blink 

//Pixy
String strRequest;
enum ENUM_STATE {NONE, IDLE, NEUTRAL, HIT};
ENUM_STATE theState[2] = {ENUM_STATE::NONE, ENUM_STATE::NONE};		//[LEFT, RIGHT]


//Receive
IRrecv irrecv(IR_RX_R_PIN);
decode_results results;


//Controller messaging
String    strRxController;		//Message from controller                                



void setup()
{
	Serial.begin(115200);

#ifdef DEBUG_THIS 
	delay(1000);
	Serial.println("Pixy Target (DEBUG)");
	Serial.print("Version: ");
	Serial.println(VERSION);
	Serial.println();	
#endif 

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

    //reset and start clock 
	hrClockCount = 0;
	hrClock.priority(128); 
	hrClock.begin(hrTimerCount, HR_CLOCK_PERIOD_IN_USEC);  
	
	//State
	StateEngine_SetNewState(ENUM_STATE::IDLE, ENUM_SIDE::LEFT);
	StateEngine_SetNewState(ENUM_STATE::IDLE, ENUM_SIDE::RIGHT);	
}


//Counter used for global timing 
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

//Cycle leds through available colors
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

//State leaving and entry code
void StateEngine_SetNewState (ENUM_STATE newState, ENUM_SIDE side)
{
	switch (newState) {

		case ENUM_STATE::NONE:
			break;

		case ENUM_STATE::IDLE:
			blinkCount[side] = hrClockCount + CLOCK_TIMEOUT_BLINK;
			bBlinkState[side] = 1;
			break; 

		case ENUM_STATE::NEUTRAL:
			if (theState[side] == ENUM_STATE::IDLE) {	
				hrClockCount = 0;	//reset clock				
				//Reset hit counters
				hitTotalBlueFirst = 0;
				hitTotalRedFirst = 0;
				hitTotalBlueSecond = 0;
				hitTotalRedSecond = 0;
			}

			hitState[side] = ENUM_HIT_STATE::HIT_NONE;	
			SetColors(hitState[side],side);								//Green
			triggerState[side] = ENUM_TRIGGERED_STATE::NOT_TRIGGERED; 	//not triggerd yet
			triggerCount[side] = hrClockCount + CLOCK_TIMEOUT_TRIGGER; 	//trigger timout

			//Send serial message that going to neutral state
			Serial.print("NEUTRAL,");
			if (side == ENUM_SIDE::LEFT) { Serial.println("LEFT"); }
			else if (side == ENUM_SIDE::RIGHT) { Serial.println("RIGHT"); }

			break; 

		case ENUM_STATE::HIT:
			hitState[side] = newHit[side];	
			SetColors(hitState[side],side);								//Blue or red		 			
			triggerState[side] = ENUM_TRIGGERED_STATE::NOT_TRIGGERED; 	//not triggerd yet
			triggerCount[side] = hrClockCount + CLOCK_TIMEOUT_TRIGGER; 	//trigger timout

			revertToNeutralCount[side] = hrClockCount + CLOCK_TIMEOUT_GO_NEUTRAL;

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

#ifdef DEBUG_THIS    
	Serial.print("state = ");
	Serial.print(theState[side]);
	Serial.print(", side = ");
	Serial.println(side);
#endif	
		
	//Parse the state 
	switch (theState[side]) {

		case ENUM_STATE::NONE:
			break;

		case ENUM_STATE::IDLE:
			//toggle
			if (hrClockCount > blinkCount[side]) {
				if (bBlinkState[side] == 1)
				{
					SetColors(hitState[side],side);	
					bBlinkState[side] = 0;
				}
				else 
				{
					SetColors(ENUM_COLOR_LED::black,side);	
					bBlinkState[side] = 1;						
				}
				blinkCount[side] = hrClockCount + CLOCK_TIMEOUT_BLINK;
			} 				
			break; 

		case ENUM_STATE::NEUTRAL:
			//check for any hit while triggered
			if 	((triggerState[side] == ENUM_TRIGGERED_STATE::TRIGGERED) && 
				 (newHit[side])) {	
				StateEngine_SetNewState(ENUM_STATE::HIT, side);

				//Increment totals
				if (newHit[side] == ENUM_HIT_STATE::HIT_BLUE) {hitTotalBlueFirst++;}
				if (newHit[side] == ENUM_HIT_STATE::HIT_RED) {hitTotalRedFirst++;}
			}
	
			break; 

		case ENUM_STATE::HIT:
			//while triggered
			if 	(triggerState[side] == ENUM_TRIGGERED_STATE::TRIGGERED) {

				//check for a new hit different than current color state
				if ((hitState[side] == ENUM_HIT_STATE::HIT_BLUE) &&
 					(newHit[side] ==  ENUM_HIT_STATE::HIT_RED)) {
					StateEngine_SetNewState(ENUM_STATE::HIT, side);				
					
					//Increment totals
					hitTotalRedSecond++;
				}
				else if ((hitState[side] == ENUM_HIT_STATE::HIT_RED) &&
 					(newHit[side] ==  ENUM_HIT_STATE::HIT_BLUE)) {
					StateEngine_SetNewState(ENUM_STATE::HIT, side);				
					
					//Increment totals
					hitTotalBlueSecond++;
				}
			}		

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


//Main loop
void loop() {

	//==CONTROLLER MESSAGE RECEIVER==
	if(Serial.available() > 0)
	{
    	strRxController = Serial.readStringUntil('\n');
    	strRxController.trim();

    	if (strRxController == "START") 
    	{
#ifdef DEBUG_THIS      		
    		Serial.println("RX:START");
#endif
    		//go to neutral state
    		if (theState[ENUM_SIDE::LEFT] == ENUM_STATE::IDLE) {
    			StateEngine_SetNewState(ENUM_STATE::NEUTRAL, ENUM_SIDE::LEFT);
    			StateEngine_Parse(ENUM_SIDE::LEFT);
    		}

    		if (theState[ENUM_SIDE::RIGHT] == ENUM_STATE::IDLE) {	    		
    			StateEngine_SetNewState(ENUM_STATE::NEUTRAL, ENUM_SIDE::RIGHT);
    			StateEngine_Parse(ENUM_SIDE::RIGHT);
    		}    		    		 				
    	}
    	else if (strRxController == "STOP") 
    	{
#ifdef DEBUG_THIS      		
    		Serial.println("RX:STOP");
#endif
    		StateEngine_SetNewState(ENUM_STATE::IDLE, ENUM_SIDE::LEFT);
			StateEngine_Parse(ENUM_SIDE::LEFT);	
    		StateEngine_SetNewState(ENUM_STATE::IDLE, ENUM_SIDE::RIGHT);
			StateEngine_Parse(ENUM_SIDE::RIGHT);
    	}
    	else if (strRxController == "SCORE") 
    	{
    		int Tot;

    		//FORMAT: #1st hits, #2nd hits, #final state
    		Serial.print("RED:");
    		Serial.print(hitTotalRedFirst);
    		Serial.print(",");
			Serial.print(hitTotalRedSecond);
    		Serial.print(",");
			Tot = 0;
			if (hitState[ENUM_SIDE::LEFT] == ENUM_HIT_STATE::HIT_RED) {Tot++;}
			if (hitState[ENUM_SIDE::RIGHT] == ENUM_HIT_STATE::HIT_RED) {Tot++;} 
			Serial.println(Tot);

    		Serial.print("BLUE:");
    		Serial.print(hitTotalBlueFirst);
    		Serial.print(",");
			Serial.print(hitTotalBlueSecond);
    		Serial.print(",");
			Tot = 0;
			if (hitState[ENUM_SIDE::LEFT] == ENUM_HIT_STATE::HIT_BLUE) {Tot++;}
			if (hitState[ENUM_SIDE::RIGHT] == ENUM_HIT_STATE::HIT_BLUE) {Tot++;} 
			Serial.println(Tot);
    	}
    	else if (strRxController == "TEST_RED")
    	{
    		StateEngine_SetNewState(ENUM_STATE::NONE, ENUM_SIDE::LEFT);
    		StateEngine_SetNewState(ENUM_STATE::NONE, ENUM_SIDE::RIGHT);    		
    		SetColors(ENUM_COLOR_LED::red,ENUM_SIDE::LEFT);
    		SetColors(ENUM_COLOR_LED::red,ENUM_SIDE::RIGHT);
    	}
    	else if (strRxController == "TEST_BLUE")
    	{
    		StateEngine_SetNewState(ENUM_STATE::NONE, ENUM_SIDE::LEFT);
    		StateEngine_SetNewState(ENUM_STATE::NONE, ENUM_SIDE::RIGHT);    		
    		SetColors(ENUM_COLOR_LED::blue,ENUM_SIDE::LEFT);
    		SetColors(ENUM_COLOR_LED::blue,ENUM_SIDE::RIGHT);
    	}
    	else if (strRxController == "TEST_GREEN")
    	{
    		StateEngine_SetNewState(ENUM_STATE::NONE, ENUM_SIDE::LEFT);
    		StateEngine_SetNewState(ENUM_STATE::NONE, ENUM_SIDE::RIGHT);    		
    		SetColors(ENUM_COLOR_LED::green,ENUM_SIDE::LEFT);
    		SetColors(ENUM_COLOR_LED::green,ENUM_SIDE::RIGHT);
    	}   
    	else if (strRxController == "TEST_RED_BLUE")
    	{
    		StateEngine_SetNewState(ENUM_STATE::NONE, ENUM_SIDE::LEFT);
    		StateEngine_SetNewState(ENUM_STATE::NONE, ENUM_SIDE::RIGHT);    		
    		SetColors(ENUM_COLOR_LED::red,ENUM_SIDE::LEFT);
    		SetColors(ENUM_COLOR_LED::blue,ENUM_SIDE::RIGHT);
    	}   
    	else if (strRxController == "TEST_BLUE_HIT")
    	{
    		newHit[ENUM_SIDE::LEFT] = ENUM_HIT_STATE::HIT_BLUE;
    		StateEngine_Parse(ENUM_SIDE::LEFT);    		
    		newHit[ENUM_SIDE::RIGHT] = ENUM_HIT_STATE::HIT_BLUE;
    		StateEngine_Parse(ENUM_SIDE::RIGHT);    		
    	}       	
    	else if (strRxController == "TEST_RED_HIT")
    	{
    		newHit[ENUM_SIDE::LEFT] = ENUM_HIT_STATE::HIT_RED;
    		StateEngine_Parse(ENUM_SIDE::LEFT);    		
    		newHit[ENUM_SIDE::RIGHT] = ENUM_HIT_STATE::HIT_RED;
    		StateEngine_Parse(ENUM_SIDE::RIGHT);    		
    	}       	
    	else if (strRxController == "TEST_BLACK")
    	{
    		StateEngine_SetNewState(ENUM_STATE::NONE, ENUM_SIDE::LEFT);
    		StateEngine_SetNewState(ENUM_STATE::NONE, ENUM_SIDE::RIGHT);    		
    		SetColors(ENUM_COLOR_LED::black,ENUM_SIDE::LEFT);
    		SetColors(ENUM_COLOR_LED::black,ENUM_SIDE::RIGHT); 		
    	}       	
    	else if (strRxController == "RESET")
    	{
    		SetColors(ENUM_COLOR_LED::green,ENUM_SIDE::LEFT);
    		SetColors(ENUM_COLOR_LED::green,ENUM_SIDE::RIGHT);    		
			StateEngine_SetNewState(ENUM_STATE::IDLE, ENUM_SIDE::LEFT);
			StateEngine_SetNewState(ENUM_STATE::IDLE, ENUM_SIDE::RIGHT);	  		
    	}     	
    	else if (strRxController == "HELP")
    	{
			Serial.println("Pixy Battle Target");
    		Serial.println("TX:");
    		Serial.println("  NEUTRAL,<LEFT/RIGHT>                   --> when going to default (green)");
    		Serial.println("  HIT,<RED/BLUE,<LEFT/RIGHT>             --> when getting 1st or 2nd hit");
    		Serial.println("  <RED/BLUE>:#1st hits,#2nd hits,#final  --> upon receiving 'SCORE' command");
    		Serial.println("RX:");
    		Serial.println("  START          --> starts the game");
    		Serial.println("  STOP           --> stops the game");
    		Serial.println("  SCORE          --> sends total hit counts");
    		Serial.println("  TEST_RED       --> sets both sides to red");
    		Serial.println("  TEST_BLUE      --> sets both sides to blue");
    		Serial.println("  TEST_GREEN     --> sets both sides to green");
    		Serial.println("  TEST_RED_BLUE  --> sets one side to red, one side to blue");
    		Serial.println("  TEST_BLUE_HIT	 --> Simulate a blue hit to both sides");	
    		Serial.println("  TEST_RED_HIT	 --> Simulate a red hit to both sides");     
    		Serial.println("  TEST_BLACK     --> Simulate no LED on both sides");		
    		Serial.println("  RESET          --> reset target (ready to receive 'START')");
    		Serial.println("  HELP           --> list of commands");
    		Serial.println("  VERSION        --> code version");    		
    		Serial.println();    	 	    	
		}
    	else if (strRxController == "VERSION")
    	{
    		Serial.println(VERSION);
    	}			
    }

	//==LEFT==
	if (theState[ENUM_SIDE::LEFT] != ENUM_STATE::IDLE) {

		//check if triggered - timeout
		if (triggerState[ENUM_SIDE::LEFT] == ENUM_TRIGGERED_STATE::NOT_TRIGGERED) {
			if (hrClockCount > triggerCount[ENUM_SIDE::LEFT]) {
				triggerState[ENUM_SIDE::LEFT] = ENUM_TRIGGERED_STATE::TRIGGERED;
			}  
		}
		

	  	//Check if hit - IR Receive 
	  	irrecv.enableIRIn(IR_RX_L_PIN); 
	  	delay(RX_WAIT_TIME);

	  	newHit[ENUM_SIDE::LEFT] = ENUM_HIT_STATE::HIT_NONE;
	  	if (irrecv.decode(&results)) {    

	    	//check if hit
	    	if (results.value == TX_MSG_BLUE) {
				newHit[ENUM_SIDE::LEFT] = ENUM_HIT_STATE::HIT_BLUE;
			}
			else if (results.value == TX_MSG_RED) {
				newHit[ENUM_SIDE::LEFT] = ENUM_HIT_STATE::HIT_RED;
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
	  	irrecv.enableIRIn(IR_RX_R_PIN); 
	  	delay(RX_WAIT_TIME);
		newHit[ENUM_SIDE::RIGHT] = ENUM_HIT_STATE::HIT_NONE;

	  	if (irrecv.decode(&results)) {    

	    	//check if hit
	    	if (results.value == TX_MSG_BLUE) {
				newHit[ENUM_SIDE::RIGHT] = ENUM_HIT_STATE::HIT_BLUE;
	    	}
			else if (results.value == TX_MSG_RED)  {
				newHit[ENUM_SIDE::RIGHT] = ENUM_HIT_STATE::HIT_RED;
			}
		
	  		irrecv.resume(); // Receive the next value 
	  	}    	
  	}
  	StateEngine_Parse(ENUM_SIDE::RIGHT);

}
