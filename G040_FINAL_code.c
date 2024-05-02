#pragma config(I2C_Usage, I2C1, i2cSensors)
#pragma config(Sensor, in1,    IR_SENSOR,      sensorReflection)
#pragma config(Sensor, dgtl1,  BUMP_RIGHT,     sensorTouch)
#pragma config(Sensor, dgtl2,  BUMP_LEFT,      sensorTouch)
#pragma config(Sensor, dgtl3,  SONAR,          sensorSONAR_mm)
#pragma config(Sensor, dgtl5,  BLINKER1,       sensorDigitalOut)
#pragma config(Sensor, dgtl8,  BLINKER2,       sensorDigitalOut)
#pragma config(Sensor, I2C_1,  ,               sensorQuadEncoderOnI2CPort,    , AutoAssign )
#pragma config(Sensor, I2C_2,  ,               sensorQuadEncoderOnI2CPort,    , AutoAssign )
#pragma config(Motor,  port1,           M_RIGHT,       tmotorVex393_HBridge, PIDControl, driveRight, encoderPort, I2C_1)
#pragma config(Motor,  port2,           DROPPER,       tmotorVex393_MC29, openLoop)
#pragma config(Motor,  port10,          M_LEFT,        tmotorVex393_HBridge, PIDControl, reversed, driveLeft, encoderPort, I2C_2)
#pragma config(DatalogSeries, 0, "IR", Sensors, Sensor, in1, 50)
#pragma config(DatalogSeries, 1, "SONAR", Sensors, Sensor, dgtl3, 50)
//*!!Code automatically generated by 'ROBOTC' configuration wizard               !!*//


// Finds the IR target, moves the robot to the target, drops the ball onto the target, then
// retreats to the edge of the arena.


// NOTE: Written for 100 character line widths.


//// //// //// //// Constants used to tweak robot performance. //// /// //// //// //// //// ////


// VEX motor rotation units per one rotation of the wheel.
#define TICKS_PER_ROTATION 655.0

// Centimeters per one rotation of the wheel.
#define CM_PER_ROTATION 32.0442

// VEX motor rotation units per degree of robot turning rotation.
#define TICKS_PER_DEGREE 5.70

// Amount of averaging to apply to raw sensor data.
#define MAX_IR_AVERAGE_COUNT 1
#define MAX_SONAR_AVERAGE_COUNT 6

// Minimum beacon value to consider a valid target.
#define MIN_BEACON_THRESHOLD 20

// Whether to slow down the robot when it starts to rotate close to the target.
#define USE_SLOWDOWN 0

// Error multiplier used to determine when beacon values are considered close.
#define BEACON_CLOSE_MULT 0.8

// Error multiplier used to determine when beacon values are matching.
#define BEACON_GOOD_ENOUGH_MULT 0.05

// Distance in mm for when the robot should start slowing its approach towards the target.
#define SONAR_CLOSE 400

// Distance threshold for when the robot should perform a retargeting scan before fully approaching
// the target.
#define SONAR_RETARGET 700

// Distance in mm for when the robot is close enough to the target to drop the ball.
#define SONAR_IN_POSITION 90

// Distance value in mm that indicates robot is too close to wall.
#define SONAR_WALL 280

// Robot state values.
#define ROBOT_RESET 0
#define ROBOT_SCAN 2
#define ROBOT_TARGET 3
#define ROBOT_DROP 6
#define ROBOT_FORWARD_A 10
#define ROBOT_FORWARD_B 11
#define ROBOT_RETREAT 80
#define ROBOT_FIND_WALL 81
#define ROBOT_PARK 96
#define ROBOT_FAIL 97
#define ROBOT_EXIT 99

// Types of walls detected.
#define WALL_NONE 0
#define WALL_FRONT 1
#define WALL_RIGHT 2
#define WALL_LEFT 3


//// //// //// //// Global state variables. //// //// //// //// //// //// //// //// //// //// ////


// NOTE: Most of these are exposed here for debug monitoring.


// Min and max IR values determined while scanning.
float max_beacon = 0;
float min_beacon = 2000;

// Timers for polling sensor values.
int beacon_timer = 0;
int sonar_timer = 0;


// Handle the IR sensor polling.
float beacon_value = 0;
short beacon_count = -1;
int beacon_last_value = 0;


// Handle the sonar sensor polling.
float sonar_value = 0;
short sonar_count = 0;

// Count the times the robot retargets itself during operation.
short retarget_count = 0;

// Holds the current robot state.
int robot_state = ROBOT_RESET;


//// //// //// //// Functions. //// /// //// //// //// //// //// //// //// //// //// //// //// ////


//	Set the movement target a certain distance forward (or backwards)
//  from the current position.
// 		distance (float): Forward travel in centimeters. Negative value is backwards travel.
//		speed (float): Percentage speed to use for movement. 0 to 100.
//		hold (bool): If true, motors will continuously hold the robot at the target position.
void setMoveTarget(float distance, float speed=70.0, bool hold=false) {
	float ticks = distance / CM_PER_ROTATION * TICKS_PER_ROTATION;

	float ticks_left = getMotorEncoder(M_LEFT) + ticks;
	float ticks_right = getMotorEncoder(M_RIGHT) + ticks;

	setMotorTarget(M_LEFT, ticks_left, speed, hold);
	setMotorTarget(M_RIGHT, ticks_right, speed, hold);
}


//	Set robot to rotate a certain angle in degrees to the left (or right) of its
//	current position.
// 		distance (float): Rotation amount in degrees to the left. Negative value indicates
//			rotation to the right.
//		speed (float): Percentage speed to use for movement. 0% to 100%.
//		hold (bool): If true, motors will continuously hold the robot at the target position.
void setRotateTarget(float angle, float speed=70.0, bool hold=false) {
	// HACK: Bad motors turn too fast to the left. This balances out the left & right turn rate.
	if (angle > 0) {
		speed *= 0.7;
	}

	float ticks = angle * TICKS_PER_DEGREE;

	float ticks_left = getMotorEncoder(M_LEFT) - ticks;
	float ticks_right = getMotorEncoder(M_RIGHT) + ticks;

	setMotorTarget(M_LEFT, ticks_left, speed, hold);
	setMotorTarget(M_RIGHT, ticks_right, speed, hold);
}


//	Modify the motor speed without chaging the current target.
//		speed (float): Percentage speed to use for movement. 0 to 100.
//		hold (bool): If true, motors will continuously hold the robot at the target position.
void changeMotorSpeed(float speed, bool hold=false) {
	float right_speed = motor[M_RIGHT] > 0 ? speed : -speed;
	float left_speed = motor[M_LEFT] > 0 ? speed : -speed;

	moveMotorTarget(M_RIGHT, 0, right_speed, hold);
	moveMotorTarget(M_LEFT, 0, left_speed, hold);
}


// 	Check if the robot is not moving.
//	Return (bool)
bool isStopped() {
	return getMotorTargetCompleted(M_LEFT) && getMotorTargetCompleted(M_RIGHT);
}


// Set the robot to a default state.
void initRobot() {
	resetMotorEncoder(M_LEFT);
	resetMotorEncoder(M_RIGHT);
	setMoveTarget(0, 0);
	SensorValue[BLINKER1] = 0;

	clearTimer(timer1);
	clearTimer(timer2);
	clearTimer(timer3);
	clearTimer(timer4);

	max_beacon = 0;
	min_beacon = 0;
	beacon_timer = 0;
	sonar_timer = 0;
	robot_state = ROBOT_RESET;
}


//  Adds a value to a floating average and returns the new average.
//		value (float): Value to add to the average.
//		average (float): Current floating average.
//		count (short*): Number of values currently forming the floating average.
//		max_count (short): Maximum number of values to use in the floating average.
//
//	Returns (float): The updated average.
float valueAverage(float value, float average, short *count, short max_count=4) {
	average = (average * (*count) + value) / (*count + 1);

	if (*count < max_count) {
		*count += 1;
	}

	return average;
}


// 	Finds the beacon value from the IR sensor. Checks the difference between IR sensor values taken
// 	at 50ms intervals to filter out background noise. Returns the floating average of the beacon
//	values.
//		average (float): Current floating average of the beacon values.
//		count (short*): Number of values currently forming the floating average.
//		last_value (int*): Last IR sensor value, required to filter out background IR from the
//			 beacon.
//
//	Returns (float): The updated floating average of beacon values. Background is < 10. Towards
//  	target is at least 30.
float getBeaconValue(float average, short *count, int *last_value) {
	if (beacon_timer < 50) {
		return average;
	}

	int value = SensorValue(IR_SENSOR);

	beacon_timer = 0;

	if (*count == -1) {
		*last_value = value;
		*count += 1;
		return average;
	}

	int difference = abs(value - *last_value);

	*last_value = value;

	return valueAverage(difference, average, count, MAX_IR_AVERAGE_COUNT);
}


// 	Gets the value from the ultrasonic/sonar sensor. Returns a floating average of the data.
//		average (float): Current floating average of the sonar values. In millimeters.
//		count (short*): Number of values currently forming the floating average.
//
//	Returns (float): Updated floating average of sonar values. In millimeters.
float getSonarValue(float average, short *count, float *slope=NULL) {
	if (sonar_timer < 10) {
		return average;
	}

	int value = SensorValue(SONAR);

	sonar_timer = 0;

	float new_average = valueAverage(value, average, count, MAX_SONAR_AVERAGE_COUNT);

	return new_average;
}


//	Update all the robot sensors.
void updateSensors() {
	// Update the individual sensor timers.
	if (time1[timer4] >= 1) {
		beacon_timer ++;
		sonar_timer ++;
		clearTimer(timer4);
	}

	// Get the IR beacon value. Background is < 10. Facing towards the target is at least 30.
	beacon_value = getBeaconValue(beacon_value, &beacon_count, &beacon_last_value);

	// Get the sonar sensor value. In mm to the closest object in front of the sensor.
	sonar_value = getSonarValue(sonar_value, &sonar_count);
}


// 	Pauses until both motors report they are not moving. Continues to update sensors.
//
//	NOTE: Sometimes gets into a state where VEX reports the motors being in a target
//		completed state, but the waitUntilMotorStop function halts forever anyways.
//		Possible VEX bug.
void waitUntilStopped() {
	while(!isStopped()) {
		updateSensors();
	}
}


//	Check the bumpers for any wall collisions.
//
// 	Return (int): WALL_LEFT or WALL_RIGHT for the corresponding collision. WALL_NONE otherwise.
int checkBumpers() {
	if (SensorValue(BUMP_LEFT) == 1) {
		return WALL_LEFT;
	}
	else if (SensorValue(BUMP_RIGHT) == 1) {
		return WALL_RIGHT;
	}
	else {
		return WALL_NONE;
	}
}


//	Detects any sensor values that indicate a wall collision.
//		sonar_value (float): The sonar value in mm to check for a wall collision.
//
//	Return (int): The type of wall collision that was detected or WALL_NONE otherwise.
int checkForWall(float sonar_value) {
	if (sonar_value <= SONAR_WALL) {
		return WALL_FRONT;
	}

	return checkBumpers();
}


//	Reset the appropriate variables and begin a scan rotation.
//		start_angle (float): Initial rotation to perform before begining to scan.
//		scan_angle (float): The rotation angle to scan.
void startScan(float start_angle, float scan_angle) {
	if (start_angle != 0) {
		setRotateTarget(start_angle, 45);
		waitUntilStopped();
	}

	setRotateTarget(scan_angle, 40);
	max_beacon = 0;
	min_beacon = 2000;
	robot_state = ROBOT_SCAN;
}


// 	Pauses a certain amount of milliseconds while continuing to update the sensors.
//		milliseconds (long): Number of milliseconds to wait.
void waitWithSensors(long milliseconds) {
	clearTimer(timer1);

	while(time1[timer1] < milliseconds) {
		updateSensors();
	}
}


// Main entrypoint function.
task main()
{
	// timer1 is used for waiting during states.
	// timer4 is used for updating the sensor timers.

	initRobot();

	// Main loop.
	while(robot_state != ROBOT_EXIT) {

		// Update sensors and check for bumper collision for any robot state.

		// Update the individual sensor timers.
		updateSensors();


		// Check if the side bumper are hit.
		int wall_hit = checkBumpers();

		// If a collision occured: stop, backup, rotate away from wall, and then start a new scan.
		if (wall_hit == WALL_LEFT) {
			setMoveTarget(0, 0);
			waitWithSensors(500);
			setMoveTarget(-15, 30);
			waitUntilStopped();
			setRotateTarget(15, 30);
			waitUntilStopped();
			setMoveTarget(-15, 30);
			waitUntilStopped();
			startScan(0, -450);
		} else if (wall_hit == WALL_RIGHT) {
			setMoveTarget(0, 0);
			waitWithSensors(1000);
			setMoveTarget(-15, 30);
			waitUntilStopped();
			setRotateTarget(-15, 30);
			waitUntilStopped();
			setMoveTarget(-15, 30);
			waitUntilStopped();
			startScan(0, -450);
		}


		// State specific logic.
		switch(robot_state) {


			// Wait 1 second, then start rotating 360 degrees to the left.
		case ROBOT_RESET: //// //// //// //// //// //// //// //// //// //// //// //// //// ////
			waitWithSensors(1000);
			startScan(0, -450);
			break;


			// Find the max and min IR sensor values while rotating.
		case ROBOT_SCAN: //// //// //// //// //// //// //// //// //// //// //// //// //// //// ////
			if (beacon_value > max_beacon) {
				max_beacon = beacon_value;
			}

			if (beacon_value < min_beacon) {
				min_beacon = beacon_value;
			}

			// Once stopped, check if the minimum beacon threshold was detected, and if so start
			// rotating back towards the target.
			if (isStopped()) {
				if (max_beacon <= MIN_BEACON_THRESHOLD) {
					robot_state = ROBOT_FAIL;
				}
				else {
					setRotateTarget(-360, 30);
					robot_state = ROBOT_TARGET;
				}
			}
			break;


			// Stop the robot once the IR sensor detects a matching max beacon value.
		case ROBOT_TARGET: //// //// //// //// //// //// //// //// //// //// //// //// //// ////

			// If the robot stops before finding the IR target, start over.
			if (isStopped()) {
				startScan(0, -450);
			}

			// Stop and signal target found when the robot is facing the direction of the max
			// beacon value.
			if (beacon_value > max_beacon - (max_beacon - min_beacon) * BEACON_GOOD_ENOUGH_MULT) {
				setMoveTarget(0, 0);
				waitWithSensors(500);

				SensorValue[BLINKER1] = 1;
				waitWithSensors(1000);
				SensorValue[BLINKER1] = 0;

				setMoveTarget(400, 50);

				if (sonar_value <= SONAR_RETARGET + 200 || retarget_count >= 1) {
					robot_state = ROBOT_FORWARD_B;
				}
				else {
					robot_state = ROBOT_FORWARD_A;
				}
			}
			else if (beacon_value > max_beacon - (max_beacon - min_beacon) * BEACON_CLOSE_MULT) {
				changeMotorSpeed(15);
			}
			break;


			// Move forward until closer to the target and then realign to the target.
		case ROBOT_FORWARD_A: //// //// //// //// //// //// //// //// //// //// //// //// //// ////
			if (isStopped() || sonar_value <= SONAR_RETARGET) {
				setMoveTarget(0, 0);
				waitWithSensors(100);
				startScan(0, -450);
				retarget_count += 1;
			}
			break;


			// Move forward until in dropping position at the target.
		case ROBOT_FORWARD_B: //// //// //// //// //// //// //// //// //// //// //// //// //// ////
			if (isStopped()) {
				waitWithSensors(1000);
				startScan(0, -450);
			}

			if (sonar_value <= SONAR_IN_POSITION) {
				setMoveTarget(0, 0);
				SensorValue[BLINKER1] = 1;
				robot_state = ROBOT_DROP;
			}
			else if (sonar_value <= SONAR_CLOSE) {
				changeMotorSpeed(30);
			}
			break;


			// Drop the ball on the target.
		case ROBOT_DROP: //// //// //// //// //// //// //// //// //// //// //// //// //// //// ////
			waitWithSensors(1000);


			motor[DROPPER] = 20;
			waitWithSensors(1100);
			motor[DROPPER] = 0;

			waitWithSensors(1000);


			SensorValue[BLINKER1] = 0;

			robot_state = ROBOT_RETREAT;

			break;


			// Move away from the target.
		case ROBOT_RETREAT: //// //// //// //// //// //// //// //// //// //// //// //// //// ////
			setMoveTarget(-30, 40);

			waitUntilStopped();
			robot_state = ROBOT_FIND_WALL;

			break;


			// Rotate towards a wall. //// //// //// //// //// //// //// //// //// //// //// ////
		case ROBOT_FIND_WALL:
			setRotateTarget(-90, 40);

			waitUntilStopped();

			setMoveTarget(200, 50);
			robot_state = ROBOT_PARK;

			break;


			// Move up to the wall of the arena then stop and flash blinker.
		case ROBOT_PARK: //// //// //// //// //// //// //// //// //// //// //// //// //// ////
			if (checkForWall(sonar_value) != WALL_NONE) {
				setMoveTarget(0, 0);
				SensorValue[BLINKER1] = 1;
				robot_state = ROBOT_EXIT;
			}
			break;


			// Failure state. Reset, blink the lights three times, then exit.
		case ROBOT_FAIL: //// //// //// //// //// //// //// //// //// //// //// //// //// //// ////
		default:
			initRobot();
			SensorValue[BLINKER1] = 0;
			for(int i = 0; i < 6; ++i) {
				SensorValue[BLINKER1] = !SensorValue[BLINKER1];
				wait1Msec(350);
			}

			robot_state = ROBOT_EXIT;
			break;
		}
	}
}
