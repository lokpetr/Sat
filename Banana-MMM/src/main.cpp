#include <Arduino.h>
#include <SD.h>

#include <CoilFlyWheel.h>
#include <GyroscopeV2.h>
#include <IS_Bluetooth.h>
#include <LightSensor.h>
#include <MagnetometerV2.h>
#include <MotorFlyWheel.h>

#include "camera.h"
#include "camera_writer.h"
#include "commands.h"
#include "opticals.h"
#include "power.h"
#include "radio.h"
#include "slidingAverage.h"
#include "telemetry.h"

using namespace IntroSatLib;
typedef unsigned char byte;

// -------------------------

// OPTICALS
#define OPTICAL_AMOUNT 4
OpticalArray opticals;

// GYROSCOPE
SlidingAverage3D<5> gyro_sliding;
#define GYRO_SENSITIVITY GyroscopeV2::Scale::DPS0500
GyroscopeV2 gyro;

// MAGNETOMETER
SlidingAverage3D<5> mag_sliding;
#define MAG_SENSITIVITY MagnetometerV2::Scale::G4
MagnetometerV2 mag;

// FLYWHEELS
#define MOTOR_1_ADDR 0x38
#define MOTOR_2_ADDR 0x3B
MotorFlyWheel motor1(MOTOR_1_ADDR);
MotorFlyWheel motor2(MOTOR_2_ADDR);
const uint16_t MOTOR_SPEED_EPS = 50;
CoilFlyWheel coil;

// RADIO
#define RADIO_TX_PIN PB12
#define RADIO_RX_PIN PB1
CustomRadio radio_tx(RADIO_TX_PIN);
CustomRadio radio_rx(RADIO_RX_PIN);

// SD
const uint8_t SD_PIN = PA4;

// CAMERA
HardwareSerial Serial3(USART3);
CustomCamera<CAMERA_WIDTH, CAMERA_HEIGHT> camera(Serial3, 115200);
CameraWriter camera_writer(SD_PIN, CAMERA_WIDTH, CAMERA_HEIGHT);

// POWER
const uint8_t SOLAR_I_PIN = PA0;
const uint8_t SOLAR_V_PIN = PA1;
const uint8_t ACCUM_I_PIN = PA3;
const uint8_t ACCUM_V_PIN = PA2;

const double voltage_LIMIT = 6.6;
const double current_LIMIT = 6.6;

// -------------------------

void setup() {
	Serial1.begin(115200, SERIAL_8E1);
	Serial1.println("serial  - success");

	Wire.begin();
	Serial1.println("wire    - success");

	if (opticals.Init())
		Serial1.println("optic   - success");
	else
		Serial1.println("optic   - fail");

	gyro.Init();
	gyro.SetScale(GYRO_SENSITIVITY);
	Serial1.println("gyro    - success");

	mag.Init();
	mag.SetScale(MAG_SENSITIVITY);
	Serial1.println("mag     - success");

	motor1.Init();
	Serial1.println("motor1  - success");
	motor2.Init();
	Serial1.println("motor2  - success");

	coil.Init();
	Serial1.println("coil    - success");

	if (radio_tx.getCC1101()) {
		radio_tx.begin();
		Serial1.println("radioTx - success");
	} else
		Serial1.println("radioTx - fail");

	if (radio_rx.getCC1101()) {
		radio_rx.begin();
		Serial1.println("radioRx - success");
	} else
		Serial1.println("radioRx - fail");

	if (SD.begin(SD_PIN))
		Serial1.println("sd      - success");
	else
		Serial1.println("sd      - fail");

	File root = SD.open("/");
	if (!root)
		Serial1.println("root    - fail");
	else
		root.close();
	
	Serial1.println("camera  - success");
}

void loop() {
	if (String command = readCommand())
		processCommand(command);

	telemetry telem;
	byte camera_buffer[CAMERA_WIDTH];
	xyz_data<float> g;
	xyz_data<float> m;

	// POWER
	telem.solar_voltage = readVoltage(SOLAR);
	telem.solar_current = readCurrent(SOLAR);
	telem.accum_voltage = readVoltage(ACCUM);
	telem.accum_current = readCurrent(ACCUM);

	// OPTICALS
	opticals.GetLight(telem.light_sensors);

	// GYROSCOPE
	g = gyro_sliding.calculate(gyro.X(), gyro.Y(), gyro.Z());

	// MAGNETOMETER
	mag.Read();
	m = mag_sliding.calculate(mag.X(), mag.Y(), mag.Z());

	camera.waitFrame();

	String frame_filename = camera_writer.findNextFileName();

	for (uint16_t i = 0; i < CAMERA_HEIGHT; i++) {
		camera.readLine(camera_buffer);
		camera_writer.appendImage(frame_filename, camera_buffer);

		radio_tx.SendData(camera_writer.toBytes(camera_buffer), CAMERA_WIDTH + 3);

		delay(100);
	}

	// radio.send("http://s91757is.beget.tech/");
	radio_tx.SendData(telem.toBytes(), 21+3);

	delay(1000);
}
