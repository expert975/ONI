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


//Clock variables
unsigned int lastClockCycleTime; //stores the last clock cycle time
unsigned int clockCycleStartTime; //stores when last clock cycle started
boolean longExecutionTime; //holds true when loop takes longer than definedClockTime

//Controller variables
byte error;
byte type;


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

//checks if the controller is properly connected
void checkController()
{
	if (controllerEnabled) //if current mode uses controller
	{
		if ()
		{
			
		}
	}
}

//Library controller detection function
void detectController()
{
	//Setup pins and settings: GamePad(clock, command, attention, data, Pressures?, Rumble?) check for error
	error = ps2x.config_gamepad(PS2_CLK, PS2_CMD, PS2_SEL, PS2_DAT, pressures, rumble);
	type = ps2x.readType();
	
	//Serial prints for controller information
	Serial.println("Detecting controller...");
	switch(error)
	{
		case 0:
			Serial.println("Found Controller, configuration successful!");
			break;
		case 1:
			Serial.println("No controller found.");
			type = 0; //when error is 1, sometimes type doesn't get updated
			break;
		case 2:
			Serial.println("Controller found but not accepting commands.");
			break;
		case 3:
			Serial.println("Controller refusing to enter Pressures mode, may not support it.");
			break;
	}
	Serial.print("Controller type: ");
	switch(type)
	{
		case 0:
			Serial.println("Unknown Controller.");
			break;
		case 1:
			Serial.println("DualShock Controller.");
			break;
		case 2:
			Serial.println("GuitarHero Controller.");
			Serial.println("This controller is not supported!");
			break;
		case 3:
			Serial.println("Wireless Sony DualShock Controller.");
			break;
	}
}