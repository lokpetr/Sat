#include <Arduino.h>

static String command;
static bool readingCommand = false;

void processCommand(const String &cmd) {
	// Serial.print("Received command: ");
	// Serial.println(cmd);

	String commandLower = cmd;
	commandLower.toLowerCase();

	if (commandLower == "command1") {
		Serial1.println("Executing Command 1");
	} else if (commandLower == "command2") {
		Serial1.println("Executing Command 2");
	} else if (commandLower == "command3") {
		Serial1.println("Executing Command 3");
	} else {
		Serial1.println("Unknown command received");
	}
}

String readCommand() {
	while (Serial1.available()) {
		byte incomingByte = Serial1.read();

		if (incomingByte == 0x00) {
			if (readingCommand) {
				command = "";
			} else {
				readingCommand = true;
				command = "";
			}
		} else if (readingCommand) {
			if (incomingByte == 0x00) {
				processCommand(command);
				readingCommand = false;
			} else
				command += incomingByte;
		}
	}

	return command;
}
