/*
  FileTransfer_Receiver.ino
  -------------------------
  Demonstrates receiving a file over CC1101 and writing it to the SD
  card using the FileTransfer module.

  Same hardware wiring as the sender (STMF103 Blue Pill, CC1101 on SPI2
  with CS on PB12, SD card CS on PA4).  The same CC1101 is used for
  both receiving DATA and sending ACKs, which works because after each
  SendData() we immediately put the radio back into RX via SetRx().

  If you use the two-radio setup from the main Banana-MMM sketch (one
  TX module on PB12, one RX module on PB1), pass each radio to the
  FileReceiver constructor, e.g.:

      CustomRadio radio_tx(PB12);
      CustomRadio radio_rx(PB1);
      FileTransfer::FileReceiver rx(radio_rx, radio_tx);

  Upload and open the serial monitor at 115200 baud, 8E1.
  Press 'r' to begin listening for a file transfer.
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

// Air bit-rate.  Must match the sender.
static const uint32_t AIR_BAUD = 4800;

CustomRadio                radio(RADIO_CS_PIN);
FileTransfer::FileReceiver receiver(radio, radio);

void setup() {
	Serial1.begin(115200, SERIAL_8E1);
	delay(200);
	Serial1.println("\n--- CC1101 File Receiver ---");

	// Radio
	if (radio.getCC1101()) {
		radio.begin();
		radio.setBaudRate(AIR_BAUD);
		receiver.setBitrate(AIR_BAUD);
		radio.SetRx();
		Serial1.println("radio  - OK");
	} else {
		Serial1.println("radio  - FAIL");
		while (1) { delay(1000); }
	}

	// SD
	if (SD.begin(SD_CS_PIN)) {
		Serial1.println("sd     - OK");
	} else {
		Serial1.println("sd     - FAIL");
		while (1) { delay(1000); }
	}

	Serial1.println("Ready. Press 'r' to wait for a file.");
}

void loop() {
	if (Serial1.available()) {
		char c = Serial1.read();
		if (c == 'r' || c == 'R') {
			Serial1.println("Listening for incoming file...");
			String outName;
			uint32_t t0 = millis();
			bool ok = receiver.receiveFile(outName, /*timeout*/ 30000);
			uint32_t dt = millis() - t0;
			if (ok) {
				Serial1.print("OK  file=\"");
				Serial1.print(outName);
				Serial1.print("\"  bytes=");
				Serial1.print(receiver.lastReceivedBytes());
				Serial1.print("  id=");
				Serial1.print(receiver.lastFileId());
				Serial1.print("  time=");
				Serial1.print(dt);
				Serial1.println(" ms");
			} else {
				Serial1.println("TIMEOUT / ERROR");
			}
		}
	}
}
