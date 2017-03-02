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
char buffer[1024]; //this is the string that holds the debug output
const boolean DEBUG_CLK_TIME = true; //weather should clock timings be written to serial

//Operational modes
const byte WAIT	= 1; //default mode at startup
const byte DRIVE = 2; //normal operation mode
const byte CALIBRATION = 3; //engine dead zone calibration mode

//Operation control
byte modusOperandi; //defines how the system should behave (i.e. current mode)
boolean controllerEnabled; //enables controller
boolean controllerMandatory; //if the mode only functions with a controller
boolean clockEnabled; //enables the clock
int definedClockTime; //how long should each clock cycle take


//Clock variables
unsigned int lastClockCycleTime; //stores the last clock cycle time
unsigned int clockCycleStartTime; //stores when last clock cycle started
boolean longExecutionTime; //holds true when loop takes longer than definedClockTime

//Controller variables
unsigned int controllerTimeOut = 2000; //how long should be an error sequence before a controller detection
unsigned int firstErrorTime = 1; //stores the beginning of an error sequence
unsigned int lastErrorTime = 1; //stores the last error occurrence
boolean validController; //stores weather the controller is valid or not
byte error; //stores error code for controller detection
byte type; //stores controller type


void setup()
{
	pinMode(systemBuzzerPin, OUTPUT); //main buzzer
	pinMode(chargerKeyPin, INPUT); //key enables charge mode
	Serial.begin(57600);
	
	detectController(); //initialize controller
	setMode(WAIT); //sets mode to wait at boot
}

void loop()
{
	clockCycleStartTime = millis(); //clock cycle start

	controllerManager(); //controller validation manager
	
	modeManager(); //call the right mode function for the current mode
	
	keySequenceManager(); //detects key sequences and combinations and changes between modes
	
	debugManager(); //prints debug information
	
	clockManager(); //clock manager
}

//Calls the current mode manager
void modeManager()
{
	switch (modusOperandi) //call the right manager depending on current operating mode
	{
		case WAIT:
			waitMode();
			break;
			
		case DRIVE:
			driveMode();
			break;
			
		case CALIBRATION:
			calibrationMode();
			break;
	}
}

//Wait mode operation
void waitMode() //what happens in wait mode?
{
	
}

//Drive mode operation
void driveMode() //what happens in drive mode?
{
	
}

//Calibration mode operation
void calibrationMode() //what happens in calibration mode?
{
	
}

//Allows modes to operate in fixed clock
void clockManager()
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

//Sets clock time
void setClock(int clockTime)
{
	if (clockTime == 0)
	{
		clockEnabled = false;
		definedClockTime = 0;
	}
	else
	{
		clockEnabled = true;
		definedClockTime = clockTime;
	}
}

//Checks if the controller is properly connected
void controllerManager()
{
	if (controllerEnabled) //if current mode uses controller
	{
		ps2x.read_gamepad(); //read controller
		if (isValidController()) //if valid controller
		{
			firstErrorTime = 1; //mark controller as valid this cycle
		}
		else //invalid readings
		{
			lastErrorTime = millis(); //store last error occurrence
			if (firstErrorTime == 1) //if controller was valid on last cycle
			{
				firstErrorTime = lastErrorTime; //store first error occurrence
			}
			else //if controller was not valid last cycle
			{
				if ((lastErrorTime - firstErrorTime) > controllerTimeOut) //if invalid readings for more than controllerTimeOut
				{
					detectController(); //controller must be unconnected, detectController()
					firstErrorTime = 1; //wait one more controllerTimeOut before next check
				}
			}
		}
	}
}

//Check data integrity
boolean isValidController ()
{
	if ((ps2x.Analog(PSS_LY) == 255 and ps2x.Analog(PSS_RX) == 255) or (ps2x.Analog(PSS_LY) == 0 and ps2x.Analog(PSS_RX) == 0))
	{
		validController = false;
		return false; //controller readings are all 255 or 0. Might be poorly connected or not connected at all
	}
	else
	{
		validController = true;
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

//Detects key sequences and combinations
void keySequenceManager()
{
	if (validController) //if controller i present
	{
		if (ps2x.ButtonPressed(PSB_R3)) //if R3 was just pressed
		{
			if (ps2x.Button(PSB_PAD_RIGHT) and ps2x.Button(PSB_SELECT)) //if right and select were pressed
			{
				setMode(CALIBRATION); //initialize calibration mode
			}
		}
	}
}

//Sets up a new operation mode
void setMode(byte newMode)
{
	if (newMode != modusOperandi) //if newMode is different from current mode
	{
		switch(newMode)
		{
			case WAIT:
				modusOperandi = WAIT;
				controllerEnabled = true; //enable controller
				setClock(50); //set clock to 50m
				break;
				
			case DRIVE:
				modusOperandi = DRIVE;
				controllerEnabled = true;
				setClock(10); //10ms clock time for maximum response
				break;
				
			case CALIBRATION:
				modusOperandi = CALIBRATION;
				controllerEnabled = true;
				setClock(50);
				for (int i=500;i<1800;i+=80) //make little noise for debugging 
					{
						tone(systemBuzzerPin, i, 30);
						delay(29);
					}
				break;
		}
	}
}

void debugManager ()
{
	if (DEBUG_CLK_TIME)
	{
		sprintf(buffer, "CLK: %u", lastClockCycleTime); //format the output string
		Serial.println(buffer);
	}
	
}

