/*
	*************************************
	*	ONI - Objeto Não Identificado	*
	*	  Written by Rodrigo Martins	*
	*		   November 15, 2015		*
	*************************************
*/

#include <PS2X_lib.h> //for v1.6 **Modified**
#include <L293D.h> // **Modified**

//PS2 controller pins 
#define PS2_DAT 14
#define PS2_CMD 15
#define PS2_SEL 16 //yellow
#define PS2_CLK 17

#define pressures 	true  //button pressures
#define rumble 		true  //controller vibration

// Hardware setup
PS2X ps2x; //starts a 'PS2 controller' object
//Starts a 'engine' object: enablePin, pinA, pinB. See L293D schematic for more details.
L293D engL(11,2,3); //left engine
L293D engR(12,7,8); //right engine
byte systemBuzzerPin = 6; //main buzzer
byte chargerKeyPin = 5; //enables charge mode


//Debug control
const boolean DEBUG_CLK_TIME = false;

//Operation control
byte modusOperandi; //defines how the system should behave
boolean controllerEnabled; //enables controller
boolean clockEnabled; //enables the clock
int definedClockTime; //how long should each clock cycle take

//Working variables
unsigned int lastClockCycleTime; //stores the last clock cycle time
unsigned int clockCycleStartTime; //stores when last clock cycle started
boolean longExecutionTime; //holds true when loop takes longer than definedClockTime


void setup()
{
	pinMode(systemBuzzerPin, OUTPUT); //main buzzer
	pinMode(chargerKeyPin, INPUT); //enables charge mode
	Serial.begin(57600);

}

void loop()
{
	//Clock cycle start
	clockCycleStartTime = millis();

	checkController(); //controller validation manager
	
	clockControl(); //clock manager
}

//Allows modes to operate in fixed clock
void clockControl()
{
	//This next function should always be the last one in the loop sequence
	if (clockEnabled) //if current mode uses clock
	{
		lastClockCycleTime = millis() - clockCycleStartTime; //processing time used in the main loop
		if (lastClockCycleTime > definedClockTime) //if loop used processing time is greater than the defined clock time 
		{
			longExecutionTime = true; //report long execution
		}
		else //if lastClockCycleTime was faster than definedClockTime...
		{
			delay(definedClockTime - lastClockCycleTime); //...we still got some time to waste
		}
	}	
}
