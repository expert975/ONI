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

#include <Arduino.h>
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
const boolean DEBUG_CONTROLLER = true; //weather should controller information be written to serial: validController LX RY
const boolean DEGUB_CONTRLLER_TYPE = false; //weather should controller type be displayed on the console at a new reconnection: output from connection attempts
const boolean DEBUG_ENGINE_MATH = true; //weather should engine math be displayed to the console: accel curve engineDeadzoneOffset calibrationBuffer curvatureSpeed*100 speedL speedR

//Operational modes
const byte WAIT	= 1; //default mode at startup
const byte DRIVE = 2; //normal operation mode
const byte CALIBRATION = 3; //engine dead zone calibration mode

//Operation control
byte modusOperandi; //defines how the system should behave (i.e. current mode)
boolean controllerEnabled; //enables controller
boolean controllerMandatory; //if the mode only functions with a controller
boolean clockEnabled; //enables the clock
unsigned int definedClockTime; //how long should each clock cycle take


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
const float TURN_RATE = 0.4; //this controls how sharp turning is, changes with velocity (0~1)
const boolean INVERT_LEFT_STICK = false; //sets controller left stick inversion
const boolean INVERT_RIGHT_STICK = true; //sets controller right stick inversion
byte engineDeadzoneOffset = EEPROM.read(0); //read calibration data from persistent storage
int accel;
int curve;
double curvatureSpeed;
float pAccel;
float pCurve;
int speedL = 0; //speed on left engine
int speedR = 0; //speed on right engine

//Calibration variables
int calibrationBuffer = engineDeadzoneOffset; //this buffer stores calibration value while calibrating

//Necessary headers:
void detectController();
void setMode(byte);
void controllerManager();
void modeManager();
void keySequenceManager();
void debugManager();
void clockManager();
void waitMode();
void calibrationMode();
void driveMode();
void engineManager();
boolean isValidController();
int mapValues(byte, boolean);


void setup()
{
	pinMode(systemBuzzerPin, OUTPUT); //main buzzer
	Serial.begin(115200);

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
		engineManager();
	}
	else
	{

	}
}

//Calibration mode operation
void calibrationMode() //what happens in calibration mode?
{
	if (validController)
	{
		if (ps2x.Button(PSB_CROSS)) //if cross is pressed, test calibration value on the engines.
		{
			engL.set(calibrationBuffer);
			engL.set(calibrationBuffer);
		}
		else
		{
			engL.set(0); //stop engines if PSB_CROSS is no longer pressed
			engL.set(0);
		}

		if (ps2x.ButtonPressed(PSB_CIRCLE)) //if circle was pressed, reset calibration buffer to 0
		{
			calibrationBuffer = 0;
		}
		else if (ps2x.ButtonPressed(PSB_SQUARE)) //if square was pressed, reset calibration buffer current value
		{
			calibrationBuffer = engineDeadzoneOffset;
		}
		else if (ps2x.ButtonPressed(PSB_PAD_UP) or ps2x.ButtonPressed(PSB_PAD_DOWN)) //up or down arrow was pressed, increase or decrease calibration buffer
		{
			byte addToBuffer = 0; //stores how much will be added to the buffer
			if (ps2x.Button(PSB_L1)) //L1 is pressed, increase by a greater amount
			{
				addToBuffer += 15;
			}
			else if (ps2x.Button(PSB_L2)) //L2 is pressed, increase by a lesser amount
			{
				addToBuffer += 1;
			}
			else //increase calibration buffer
			{
				addToBuffer += 5;
			}

			if (ps2x.ButtonPressed(PSB_PAD_UP)) //if arrow up was pressed, increase buffer
			{
				calibrationBuffer += addToBuffer;
			}
			else //if arrow down was pressed, decrease buffer
			{
				calibrationBuffer -= addToBuffer;
			}
			calibrationBuffer = constrain(calibrationBuffer, 0, 255); //keep calibration buffer inside pwm range
		}
	}
	else
	{

	}
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
	else if (ps2x.Analog(PSS_LY) == 115 and ps2x.Analog(PSS_RX) == 115)
	{
		tone(systemBuzzerPin, 540, 1000); //sound warning buzzer
		validController = false;
		return false; //controller readings are all 115. This usually happens when high logic voltage level falls down. Low battery
	}
	else
	{
		validController = true;
		return true;
	}
	//corner case: sometimes when high logic voltage goes really low, all buttons can go to 1 when analogs aren't 0 or 255
	//could  be detected by checking one or more buttons (e.g. PSB_START). When such happens, the system might sometimes
	// switch between forward and backwards each cycle or behave strangely, similarly to when analogs are on 115
}

//Library controller detection function
void detectController()
{
	//Setup pins and settings: GamePad(clock, command, attention, data, Pressures?, Rumble?) check for error
	error = ps2x.config_gamepad(PS2_CLK, PS2_CMD, PS2_SEL, PS2_DAT, false, false);
	type = ps2x.readType();

	//Serial prints for controller information
	if (DEGUB_CONTRLLER_TYPE)
	{
		switch(error) //prints out controller state
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
		switch(type) //prints out controller type
		{
			case 0:
				Serial.println(F("Controller type: Unknown Controller."));
				break;
			case 1:
				Serial.println(F("Controller type: DualShock Controller."));
				break;
			case 2:
				Serial.println(F("Controller type: GuitarHero Controller."));
				Serial.println(F("This controller is not supported!"));
				break;
			case 3:
				Serial.println(F("Controller type: Wireless Sony DualShock Controller."));
				break;
		}
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
				if (ps2x.Button(PSB_R2) and ps2x.Button(PSB_R1) and modusOperandi == DRIVE) //if on DRIVE mode and L1 and L2 are pressed
				{
					if (EEPROM.read(0) != engineDeadzoneOffset) //and current calibration data is different from stored on EEPROM
					{
						EEPROM.update(0, engineDeadzoneOffset); //update EEPROM with new calibration value (EEPROM <3)
						Serial.println("Writing calibration to EEPROM"); //write new data to EEPROM
						tone(systemBuzzerPin, 880.00, 200);
						delay(200);
						tone(systemBuzzerPin, 1046.50, 1000);
					}
					else
					{
						Serial.println("No new data to write!");
					}
				}
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
				if (ps2x.Button(PSB_R2)) //entered drive mode with R2 pressed
				{
					if (calibrationBuffer != engineDeadzoneOffset) //the old calibration data is different from new
					{
						Serial.println("Using new calibration value");
						engineDeadzoneOffset = calibrationBuffer; //use new calibration data
						tone(systemBuzzerPin, 2800, 30); //modify the sound so it states the change
						delay(100);
					}
					else
					{
						Serial.println("No new calibration data!");
					}
				}
				tone(systemBuzzerPin, 2800, 50);
				delay(100);
				tone(systemBuzzerPin, 2800, 250);
				delay(250);
				tone(systemBuzzerPin, 2000, 50);
				delay(50);
				tone(systemBuzzerPin, 2200, 50);
				delay(50);
				tone(systemBuzzerPin, 1500, 50);
				delay(50);
				tone(systemBuzzerPin, 3000, 50);
				break;

			case CALIBRATION:
				modusOperandi = CALIBRATION;
				controllerEnabled = true;
				setClock(50);
				calibrationBuffer = engineDeadzoneOffset; //set calibration buffer to current calibration value
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
		sprintf(buffer, "%s %i %03u %03u ", buffer, validController, ps2x.Analog(PSS_LX), ps2x.Analog(PSS_RY)); //append to the buffer
	}
	if (DEBUG_ENGINE_MATH)
	{
		sprintf(buffer, "%s %+04i %+04i %+04i %+04i %+04i %+04i %+04i ", buffer, accel, curve, engineDeadzoneOffset, calibrationBuffer, int(curvatureSpeed*100), speedL, speedR);
	}
	Serial.println(buffer); //print the debug string
}

void engineManager()
{
	curve = mapValues(ps2x.Analog(PSS_LX), INVERT_LEFT_STICK); //curves -> horizontal axis, left stick
	accel = mapValues(ps2x.Analog(PSS_RY), INVERT_RIGHT_STICK); //acceleration -> vertical axis, right stick

	//Set speed according to accel readings. Set curvatureSpeed to 0 in case of no curves
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
	{
		curvatureSpeed = float(map(int(((1 - pow(fabsf(pAccel),fabsf(pCurve))) + float(map(TURN_RATE*fabsf(pCurve)*100 - TURN_RATE*fabsf(pAccel)*50,-TURN_RATE*50,TURN_RATE*100,0,TURN_RATE*100))/100)*100),0,100 + TURN_RATE*100,0,100))/100; //really complicated stuff. There's a picture attached to the source code explaining this.
		curvatureToSpeed = map(curvatureSpeed*100,0,100,accel,-accel); //when curvatureSpeed is 0, no curves. When 50, one wheel stops. When 100, this wheel spins at the same speed that the accel, but reverse.
		//probably should not convert into percentages then out
		curvatureToSpeedReversed = -map(curvatureSpeed*100,0,100,-accel,accel); //when curvatureSpeed is 0, no curves. When -50, one wheel stops. When -100, this wheel spins at the same speed that the accel, but reverse.
	}
	//Setting speeds
	//For accel > 0 and accel < 0 speedR and speedL get set to the same values, just reversed. It might be possible to remove these statements by incorporating these cases to the main formula
	if (accel > 0) //going forward
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
	// engR.set(speedR);
	// engL.set(speedL);
	digitalWrite(systemBuzzerPin, ps2x.Button(PSB_R3)); //control buzzer based on R3 state
}

int mapValues(byte value, boolean invert)
{
	//This function should get the values from the analog axis and convert to PWM values for engine power control
	if (value == 128) //if analogs are in the middle position
	{
		return 0; //no movement
	}
	else
	{
		if (invert)
		{
			return int(-value*2 + 128*2 - 1); //maps value to range from 0~255 to 255~(-255), inverted
		}
		else
		{
			return int(+value*2 - 128*2 + 1); //maps value to range from 0~255 to -255~255
		}
	}
}

/*
//map() function:
long map(long x, long in_min, long in_max, long out_min, long out_max)
{
  return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

*/
