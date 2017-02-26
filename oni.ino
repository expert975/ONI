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
#define rumble 		false  //controller vibration

// Hardware setup
PS2X ps2x; //starts a 'PS2 controller' object

//Starts a 'engine' object: enablePin, pinA, pinB. See L293D schematic for more details.
L293D engL(11,2,3); //left engine
L293D engR(12,7,8); //right engine

byte systemBuzzerPin = 9; //main buzzer
byte chargerKeyPin = 51; //enables charge mode


//Debug control
const boolean DEBUG_CLK_TIME = false;

//Operation control
byte modusOperandi; //defines how the system should behave
boolean controllerEnabled; //enables controller
boolean controllerMandatory; //if the mode only functions with a controller
boolean clockEnabled; //enables the clock
int definedClockTime; //how long should each clock cycle take


//Clock variables
unsigned int lastClockCycleTime; //stores the last clock cycle time
unsigned int clockCycleStartTime; //stores when last clock cycle started
boolean longExecutionTime; //holds true when loop takes longer than definedClockTime

//Controller variables
unsigned int controllerTimeOut = 1000; //how long should be an error sequence before a controller detection
unsigned int firstErrorTime = 1; //stores the beginning of an error sequence
unsigned int lastErrorTime = 1; //stores the last error occurrence
byte error; //stores error code for controller detection
byte type; //stores controller type


void setup()
{
	pinMode(systemBuzzerPin, OUTPUT); //main buzzer
	pinMode(chargerKeyPin, INPUT); //enables charge mode
	Serial.begin(57600);
	
	//Temporary
	delay(1000);
	detectController();
	clockEnabled = true;
	controllerEnabled = true;
	definedClockTime = 50;

}

void loop()
{
	//Clock cycle start
	clockCycleStartTime = millis();

	checkController(); //controller validation manager
	// Serial.print("Took: ");
	// Serial.print(lastClockCycleTime);
	// Serial.print(" Millis: ");
	// Serial.println(millis());
	
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

//Checks if the controller is properly connected
void checkController()
{
	if (controllerEnabled) //if current mode uses controller
	{
		ps2x.read_gamepad(); //read controller
		if (validController()) //if valid controller
		{
			firstErrorTime = 1; //mark controller as valid this cycle
		}
		else //invalid readings
		{
			lastErrorTime = millis(); //store last error occurrence
			if (firstErrorTime == 1) //if controller was valid on last cycle
			{
				firstErrorTime = millis(); //store first error occurrence
			}
			else //if controller was not valid last cycle
			{
				if (lastErrorTime - firstErrorTime > controllerTimeOut) //if invalid readings for more than controllerTimeOut
				{
					detectController(); //controller must be unconnected, detectController()
					firstErrorTime == 1; //wait one more controllerTimeOut before next check
				}
			}
		}
	}
}

//Check data integrity
boolean validController ()
{
	if (ps2x.Analog(PSS_LY) == 255 and ps2x.Analog(PSS_RX) == 255) or (ps2x.Analog(PSS_LY) == 0 and ps2x.Analog(PSS_RX) == 0)
	{	
		return false; //controller readings are all 255 or 0. Might be poorly connected or not connected at all
	}
	else
	{
		return true;
	}
}

//Library controller detection function
void detectController()
{
	// Serial.println("BEBUG: detectController()");
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