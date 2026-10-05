#include "camera_writer.h"

CameraWriter::CameraWriter(uint16_t width, uint16_t height) {
	this->width = width;
	this->height = height;
}
CameraWriter::CameraWriter(uint8_t pin, uint16_t width, uint16_t height) {
	this->width = width;
	this->height = height;
	this->pin = pin;

	if (SD.begin(pin))
		Serial1.println("camwr  - success");
	else
		Serial1.println("camwr  - fail");
}

String CameraWriter::findNextFileName() {
	uint16_t fileIndex = 0;
	File root = SD.open("/");

	while (true) {
		File entry = root.openNextFile();
		if (!entry) {
			break;
		}

		String name = entry.name();
		if (name.startsWith("OUTPUT_")) {
			int index = name.substring(7, name.length() - 4).toInt();
			if (index >= fileIndex)
				fileIndex = index + 1;
		}
		entry.close();
	}
	root.close();
	return "output_" + String(fileIndex) + ".bin";
}

bool CameraWriter::appendImage(String filename, uint8_t *data) {
	File file = SD.open(filename, FILE_WRITE);

	if (!file)
		return false;

	file.write(data, this->width);
	file.close();

	return true;
}

byte *CameraWriter::toBytes(uint8_t *data) {
	this->packet[0] = 0b01000000 | (width >> 4);
	this->packet[1] = ((width << 4) & 0xff) | (height >> 6);
	this->packet[2] = ((height << 2) & 0xff) | frame_num;

	memcpy(this->packet + 3, data, width);

	return this->packet;
}
