/*	
	ONI - Objeto Não Identificado
	Copyright 2015, 2017 Rodrigo Martins
	
	This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <PS2X_lib.h> //for v1.6 **Modified**
#include <L293D.h> // **Modified**
#include <EEPROM.h> //allows reading and writing from EEPROM

//PS2 controller pins 
#define PS2_DAT 14
#define PS2_CMD 15
#define PS2_SEL 16 //yellow
#define PS2_CLK 17

// Hardware setup
PS2X ps2x; //starts a 'PS2 controller' object

//Starts a 'engine' object: enablePin, pinA, pinB. See L293D schematic for more details.
L293D engL(11,2,3); //left engine
L293D engR(12,7,8); //right engine

const byte systemBuzzerPin = 9; //main buzzer

//Debug control
char buffer[128]; //this is the string that holds the debug output
const boolean DEBUG_CLK_TIME = true; //weather should clock timings be written to serial: lastClockCycleTime
const boolean DEBUG_MODE = true; //weather should the current mode be written to the serial: mode
const boolean  DEBUG_CONTROLLER = true; //weather should controller information be written to serial: validController LX RY

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
const unsigned int CONTROLLER_TIMEOUT = 2000; //how long should be an error sequence before a controller detection
unsigned int firstErrorTime = 1; //stores the beginning of an error sequence
unsigned int lastErrorTime = 1; //stores the last error occurrence
boolean validController; //stores weather the controller is valid or not
byte error; //stores error code for controller detection
byte type; //stores controller type

//Engine math variables
byte engineDeadzoneOffset = EEPROM.read(0); //read calibration data from persistent storage
int accel;
int curve;
double curvatureSpeed;
float pAccel;
float pCurve;
int speedL = 0; //speed on left engine
int speedR = 0; //speed on right engine
const float turnRate = 0.4; //this controls how sharp turning is, changes with velocity (0~1)


void setup()
{
	pinMode(systemBuzzerPin, OUTPUT); //main buzzer
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
	if (validController)
	{
		
	}
	else
	{
		
	}
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
				if ((lastErrorTime - firstErrorTime) > CONTROLLER_TIMEOUT) //if invalid readings for more than the timeout
				{
					detectController(); //controller must be unconnected, detectController()
					firstErrorTime = 1; //wait one more timeout before next check
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
	error = ps2x.config_gamepad(PS2_CLK, PS2_CMD, PS2_SEL, PS2_DAT, false, false);
	type = ps2x.readType();
	
	//Serial prints for controller information
	Serial.println(F("Detecting controller..."));
	switch(error)
	{
		case 0:
			Serial.println(F("Found Controller, configuration successful!"));
			break;
		case 1:
			Serial.println(F("No controller found."));
			type = 0; //when error is 1, sometimes type doesn't get updated
			break;
		case 2:
			Serial.println(F("Controller found but not accepting commands."));
			break;
		case 3:
			Serial.println(F("Controller refusing to enter Pressures mode, may not support it."));
			break;
	}
	Serial.print(F("Controller type: "));
	switch(type)
	{
		case 0:
			Serial.println(F("Unknown Controller."));
			break;
		case 1:
			Serial.println(F("DualShock Controller."));
			break;
		case 2:
			Serial.println(F("GuitarHero Controller."));
			Serial.println(F("This controller is not supported!"));
			break;
		case 3:
			Serial.println(F("Wireless Sony DualShock Controller."));
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
			else //if right and select were not pressed
			{
				setMode(DRIVE); //initialize drive mode
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
				setClock(50); //50ms clock time. Setting to 10 ms seemed to cause problems in controller connection
				tone(systemBuzzerPin, 2800, 50);
				delay(100);
				tone(systemBuzzerPin, 2800, 250);
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

//Shows debug information relating the most relevant system parameters
void debugManager ()
{
	buffer[0] = '\0'; //clear the buffer by setting the first char as null
	if (DEBUG_CLK_TIME)
	{
		sprintf(buffer, "%3u ", lastClockCycleTime); //format the output string
	}
	if (DEBUG_MODE)
	{
		sprintf(buffer, "%s %u ", buffer, modusOperandi);
	}
	if (DEBUG_CONTROLLER)
	{
		sprintf(buffer, "%s %i %03u %03u", buffer, validController, ps2x.Analog(PSS_LX), ps2x.Analog(PSS_RY)); //append to the buffer
	}
	Serial.println(buffer); //print the debug string
}

void moveEngines()
{
	curve = mapValues(ps2x.Analog(PSS_LX), false); //curves -> horizontal axis, left stick
	accel = mapValues(ps2x.Analog(PSS_RY), true); //acceleration -> vertical axis, right stick
	
	//Set speed according to accel reading and curvatureSpeed to 0 in case of no curves
	speedL = accel; 
	speedR = accel;
	
	curvatureSpeed = 0;
	int curvatureToSpeed = 0; //could use a byte?
	int curvatureToSpeedReversed = 0; //could use a byte?
	pAccel = float(accel)/255; //divide by maximum possible value to get a percentage number
	pCurve = float(curve)/255; //1 = 100%, 0.5 = 50%
	 //might be able to optimize speed by not using this percentage in the engine math

	//Calculate curvatureSpeed only if there is accel and curve
	if (curve != 0 and accel != 0)
		curvatureSpeed = float(map(int(((1 - pow(fabsf(pAccel),fabsf(pCurve))) + float(map(turnRate*fabsf(pCurve)*100 - turnRate*fabsf(pAccel)*50,-turnRate*50,turnRate*100,0,turnRate*100))/100)*100),0,100 + turnRate*100,0,100))/100; //really complicated stuff. There's a picture attached to the source code explaining this.
		curvatureToSpeed = map(curvatureSpeed*100,0,100,accel,-accel); //when curvatureSpeed is 0, no curves. When 50, one wheel stops. When 100, this wheel spins at the same speed that the accel, but reverse. 
		//probably should not convert into percentages then out
		curvatureToSpeedReversed = -map(curvatureSpeed*100,0,100,-accel,accel); //when curvatureSpeed is 0, no curves. When -50, one wheel stops. When -100, this wheel spins at the same speed that the accel, but reverse. 
	
	//Setting speeds
	//For accel > 0 and accel < 0 speedR and speedL get set to the same values, just reversed. It might be possible to remove these statements by incorporating these cases to the main formula
	else if (accel > 0) //going forward
	{
		//For turning
		if (curve > 0) //going right
		{
			speedR = curvatureToSpeed;
		}
		else if (curve < 0) //going left
		{
			speedL = curvatureToSpeed;
		}
	}
	else if (accel < 0) //going reverse
	{
		//For turning
		if (curve > 0) //going right
		{
			speedR = curvatureToSpeedReversed;
		}
		else if (curve < 0) //going left
		{
			speedL = curvatureToSpeedReversed;
		}
	}
	engR.set(speedR);
	engL.set(speedL);
	digitalWrite(systemBuzzerPin, ps2x.Button(PSB_R3)); //control buzzer based on R3 state
}

int mapValues(byte value, boolean invert)
{
	//This function should get the values from the analog axis and convert to PWM values for engine power control
	if (value < 128) //it may be possible to replace this conditional statement for more abrangent mapping function. It might be possible to use simpler math for this mapping by expanding the map function.
	{
		if(invert == false)
			return int(map(value,0,128,-255, 0));
		else
			return int(map(value,0,128,-255, 0))*-1; //swap 0 and 128 or -255 and 0 should replace the *-1 operation, saving time. Can a byte be used instead of an integer?
	}
	else if (value > 128)
	{
		if (invert == false)
			return int(map(value,128,255, 0,255));
		else
			return int(map(value,128,255, 0,255))*-1; //same as above
	}
	else
		return 0;
}

/*
//map() function:
long map(long x, long in_min, long in_max, long out_min, long out_max)
{
  return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

*/
