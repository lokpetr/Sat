/*
  FileTransfer_Sender.ino
  -----------------------
  Demonstrates sending a file from an SD card over CC1101 using the
  FileTransfer module.

  Intended for the STM32 Blue Pill (STM32F103C8) used in the Banana-MMM
  satellite project.  The CC1101 is wired to the hardware SPI2 bus
  (PB13 SCK, PB14 MISO, PB15 MOSI) with CS on PB12.  The CC1101 is
  clocked by its on-board piezoelectric crystal oscillator (typically
  26 MHz), which the driver uses to synthesize the 433.92 MHz carrier.

  Wiring summary:
    CC1101 VCC  -> 3.3V
    CC1101 GND  -> GND
    CC1101 SCK  -> PB13
    CC1101 MISO -> PB14
    CC1101 MOSI -> PB15
    CC1101 CSN  -> PB12
    SD card CS  -> PA4 (as in main project)

  Upload and open the serial monitor at 115200 baud, 8E1.
  Press 's' to send "payload.bin" from the SD card.
  Press 't' to send a short in-memory text as "hello.txt".
*/

#include <Arduino.h>
#include <SD.h>
#include <ELECHOUSE_CC1101_SRC_DRV.h>
#include <SPI.h>

#include "radio.h"
#include "file_transfer.h"

// Pins
#define RADIO_CS_PIN  PB12
#define SD_CS_PIN     PA4

// Air bit-rate.  Must match on the receiver.  4800 bps is conservative
// and reliable through the CC1101 + piezo-crystal oscillator chain.
static const uint32_t AIR_BAUD = 4800;

CustomRadio              radio(RADIO_CS_PIN);
FileTransfer::FileSender sender(radio);

void setup() {
	Serial1.begin(115200, SERIAL_8E1);
	delay(200);
	Serial1.println("\n--- CC1101 File Sender ---");

	// Radio
	if (radio.getCC1101()) {
		radio.begin();
		radio.setBaudRate(AIR_BAUD);
		sender.setBitrate(AIR_BAUD);
		radio.SetRx();                // start in RX so we can hear ACKs
		Serial1.println("radio  - OK");
	} else {
		Serial1.println("radio  - FAIL");
		while (1) { delay(1000); }
	}

	// SD
	if (SD.begin(SD_CS_PIN)) {
		Serial1.println("sd     - OK");
	} else {
		Serial1.println("sd     - FAIL (continuing without SD)");
	}

	Serial1.println("Ready. Press 's' to send payload.bin, 't' for hello.txt.");
}

void loop() {
	if (Serial1.available()) {
		char c = Serial1.read();
		if (c == 's' || c == 'S') {
			const char *fn = "payload.bin";
			Serial1.print("Sending ");
			Serial1.print(fn);
			Serial1.println(" ...");

			uint32_t t0 = millis();
			bool ok = sender.sendFile(fn, /*fileId=*/0);
			uint32_t dt = millis() - t0;

			if (ok) {
				Serial1.print("OK in ");
				Serial1.print(dt);
				Serial1.print(" ms, retries=");
				Serial1.print(sender.lastRetries());
				Serial1.print(", packets=");
				Serial1.println(sender.lastPackets());
			} else {
				Serial1.println("FAILED");
			}
		}
		if (c == 't' || c == 'T') {
			const char *msg = "Hello from CC1101 piezo-crystal radio!";
			Serial1.println("Sending buffer as 'hello.txt'...");
			uint32_t t0 = millis();
			bool ok = sender.sendBuffer("hello.txt", 1,
			                            (const uint8_t *)msg, strlen(msg));
			uint32_t dt = millis() - t0;
			Serial1.print(ok ? "OK" : "FAILED");
			Serial1.print(" in ");
			Serial1.print(dt);
			Serial1.println(" ms");
		}
	}
}
