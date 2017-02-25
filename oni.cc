/*
		*******************************
        *	*ONI - Objeto Não Identificado*		*
        *     Written by: Rodrigo Martins			*
        *		   November 15, 2015					*
        *					v1.01								*
        *******************************
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
const int buzzerPin = 6; //buzzer pin
const int warningPin = 13; //this pin lights up when a loop takes longer than it should. If this happens a lot of times, inputs may appear laggy. False positive for fast reconnections and some weird semi-connected states, reset after new controller configuration.
byte oldError = 255; //started at -1 to mark the first time detectController() runs
byte oldType = 255;
const int checkInterval = 50; //time between controller detections (milliseconds)
unsigned long previousCheck = 0; //stores last time the controller was checked
unsigned long loopStartTime = 0; //stores the time when the loop started
const unsigned int loopTime = 50; //the total loop time (milliseconds)
boolean validController = false; //if a controller is valid
int speedL = 0; //speed on left engine
int speedR = 0; //speed on right engine
const float turnRate = 0.4; //this controls how sharp turning is, changes with velocity (0~1)
int accel = 0; //mapped value that defines the speed of both engines
int curve = 0; //mapped value that changes the speed of one engine to turn
float pAccel = 0; //stores the percentage of accel to its maximum
float pCurve = 0; //stores the percentage of curve to its maximum
double curvatureSpeed = 0; //stores the ratio between accel and curve, defines  how sharp a turn is


//Starts a 'engine' object: enablePin, pinA, pinB. See L293D schematic for more details.
L293D engL(11,2,3); //left engine
L293D engR(12,7,8); //right engine

PS2X ps2x; //starts a 'PS2 controller' object


void setup()
{
	Serial.begin(57600); //serial for debugs 'n stuff
	detectController(); //check for controller on start up
	pinMode(warningPin, OUTPUT); //pin for waning lag input caused by exceeded loop time
	pinMode(buzzerPin, OUTPUT); //buzzer pin
}

void loop()
{
	//Time control for function executions
	loopStartTime = millis();

	//Do a controller check on intervals
	if  ((loopStartTime - previousCheck >= checkInterval) or (checkInterval == loopTime))
	{
		previousCheck = loopStartTime;
		detectController();
	}

	//Skip loop if a supported controller was not found
	if (!validController)
	{
		engR.set(0); //engines are stopped so they don't freak out when a controlled is unconnected in a weird way
		engL.set(0); //for some reason, sometimes controller values are read after the controller is unconnected, resulting in some messed up data
		return;
	}

	//Read controller and set vibration speed for large motor based on blue 'X' pressure.
	ps2x.read_gamepad(false,ps2x.Analog(PSAB_CROSS));

	//Here comes the moves
	moveEngines(true); //boolean for actually moving engines. If false, does all the math, but doesn't move the engines at the end

	//Prints input, curvature math data and motor values
	debugStickPrint(false); //boolean activates it

	//This next function should always be the last one
	loopTimer(false); //this function controls loop time. Boolean is for printing statistics
}

void loopTimer(boolean printLoopTimeStatistics)
{
	//This function ensures that loop() runs every loopTime milliseconds
	unsigned int loopDelay = millis() - loopStartTime; //processing time used in the loop
	if (printLoopTimeStatistics == true)
		Serial.println(loopDelay);
	if (loopDelay > loopTime) //if loop used processing time is greater than the defined loop time
	{
		Serial.print("<<WARNING>> ");
		Serial.print("**LOOP DELAY > LOOP TIME**"); //shouldn't happen, as it might cause input lag
		Serial.println(" <<WARNING>>");
		digitalWrite(warningPin,HIGH); //turn a high signal on the warn pin once it happens.
	}
	else //if loopDelay was faster than loopTime...
	{
		delay(loopTime - loopDelay); //...we still got some time to waste
	}
}

void debugStickPrint(boolean print)
{
	if (!print) //are we supposed to spam serial monitor and waste CPU time? If not, stop right here
		return;

	//Controller readings
	Serial.print("Analog inputs: LX: ");
	Serial.print(ps2x.Analog(PSS_LX), DEC);
	Serial.print(" LY: ");
	Serial.print(ps2x.Analog(PSS_LY), DEC);
	Serial.print(" RX: ");
	Serial.print(ps2x.Analog(PSS_RX), DEC);
	Serial.print(" RY: ");
	Serial.print(ps2x.Analog(PSS_RY), DEC);
	Serial.print("; Map");
	Serial.print("(");
	Serial.print(curvatureSpeed);
	Serial.print(")Curve: ");
	Serial.print(curve);
	Serial.print("(");
	Serial.print(pCurve);
	Serial.print(")");
	Serial.print(" Accel: ");
	Serial.print(accel);
	Serial.print("(");
	Serial.print(pAccel);
	Serial.print(")");
	Serial.print("; speedL: ");
	Serial.print(speedL);
	Serial.print(" speedR: ");
	Serial.println(speedR);
}

void detectController()
{
	// Serial.println("BEBUG: detectController()");
	//Setup pins and settings: GamePad(clock, command, attention, data, Pressures?, Rumble?) check for error
	byte error = ps2x.config_gamepad(PS2_CLK, PS2_CMD, PS2_SEL, PS2_DAT, pressures, rumble);
	byte type = ps2x.readType();
	if (error == 1)
		type = 0; //when error is 1, sometimes type doesn't get updated
	// Serial.print(error);
	//Serial.println(type);
	if (oldError == 255 and error == 1) //just turned on, no controller detected
	{
		Serial.println("Please connect a controller.");
		oldError = error;
	}
	else if ((oldError == 1 and error != 1) or (oldError != 1 and error == 1)) //prints if a controller was connected or disconnected
	{
		if (oldError == 1 and error !=1) //in case a controller was connected, ignore configuration time and reset warning pin.
		{
			loopStartTime = millis(); //a false warning still happens if a controller is reconnected fast.
			digitalWrite(warningPin,LOW); //new controller, fresh and new. Let's pretend that never happened, shall we?
		}
		//Serial prints for controller information
		Serial.println("Detecting controller...");
		if (error == 0)
		{
			Serial.println("Found Controller, configuration successful!");
		}
		else if (error == 1)
		{
			Serial.println("No controller found.");
		}
		else if (error == 2)
			Serial.println("Controller found but not accepting commands.");
		else if (error == 3)
			Serial.println("Controller refusing to enter Pressures mode, may not support it.");

		Serial.print("Controller type: ");
		switch(type)
		{
			case 0:
				Serial.print("Unknown");
				break;
			case 1:
				Serial.print("DualShock");
				break;
			case 2:
				Serial.print("GuitarHero");
				break;
			case 3:
				Serial.print("Wireless Sony DualShock");
				break;
		}
		Serial.println(" Controller.");
		if (type == 2)
		{
			Serial.println("This controller is not supported!");
		}
		oldError = error;
		oldType = type;
	}

	//Set validController variable
	if (error == 1 or error == 2 or type == 2) //if not found, not accepting commands or guitar hero, then controller is not valid
		validController = false;
	else
		validController = true;


}

void moveEngines(boolean move)
{
	curve = mapValues(ps2x.Analog(PSS_LX), false); //curves -> horizontal axis, left stick
	accel = mapValues(ps2x.Analog(PSS_RY), true); //acceleration -> vertical axis, right stick

	//Set speed according to accel reading and curvatureSpeed to 0 in case of no curves
	speedL = accel;
	speedR = accel;
	curvatureSpeed = 0;
	int curvatureToSpeed = 0;
	int curvatureToSpeedReversed = 0;
	pAccel = float(accel)/255; //divide by maximum possible value to get a percentage number
	pCurve = float(curve)/255; //1 = 100%, 0.5 = 50%

	//Calculate curvatureSpeed only if there is accel and curve
	if (curve != 0 and accel != 0)
		curvatureSpeed = float(map(int(((1 - pow(fabsf(pAccel),fabsf(pCurve))) + float(map(turnRate*fabsf(pCurve)*100 - turnRate*fabsf(pAccel)*50,-turnRate*50,turnRate*100,0,turnRate*100))/100)*100),0,100 + turnRate*100,0,100))/100; //really complicated stuff. There's a picture attached to the source code explaining this.
		curvatureToSpeed = map(curvatureSpeed*100,0,100,accel,-accel); //when curvatureSpeed is 0, no curves. When 50, one wheel stops. When 100, this wheel spins at the same speed that the accel, but reverse.
		curvatureToSpeedReversed = -map(curvatureSpeed*100,0,100,-accel,accel); //when curvatureSpeed is 0, no curves. When -50, one wheel stops. When -100, this wheel spins at the same speed that the accel, but reverse.

	//Setting speeds
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

	//Actually move or just do math?
	if (move)
	{
		engR.set(speedR);
		engL.set(speedL);
	}

	digitalWrite(buzzerPin, ps2x.Button(PSB_R3)); //control buzzer based on R3 state
}

int mapValues(byte value, boolean invert)
{
	//This function should get the values from the analog axis and convert to PWM values for engine power usage
	if (value < 128)
	{
		if(invert == false)
			return int(map(value,0,128,-255,0));
		else
			return int(map(value,0,128,-255,0))*-1;
	}
	else if (value > 128)
	{
		if (invert == false)
			return int(map(value,128,255,0,255));
		else
			return int(map(value,128,255,0,255))*-1;
	}
	else
		return 0;
}





/* Crazy math explanation stub (check attached photo)
accel = Right joystick input, vertial axis
curve = Left joystick input, horizontal axis
pAccel = percentage of accel
pCurve = percentage of curve
turnRate = calibration variable to tune the turns based on empirical observations
curvatureSpeed = how sharp the turning is, its meant to depend on accel, curve and turnRate

first data goes down when pAccel goes up.
	    goes   up   when pCurve goes up.
min = 0: when pAccel is at maximum, that ignores pCurve.
max = 1: when pAccel is close to 0(no curve if no speed) and pCurve is at maximum

second data gets mapped: map(-turnRate/2), turnRate, 0, turnRate) This way the negative part of the set is removed.

final value gets mapped
map(0, 1stMax + 2ndMax, 0, 100)

multiplications and divisions by 100 are meant to convert from value to percentage of the value, wich goes to int and float/double data types, respectively
 */
