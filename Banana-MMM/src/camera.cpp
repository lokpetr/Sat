#include "camera.h"

template <uint16_t width, uint16_t height>
CustomCamera<width, height>::CustomCamera(HardwareSerial p_ser, uint32_t baud_rate) : ser(p_ser) {
	ser.begin(baud_rate);
}

template <uint16_t width, uint16_t height>
void CustomCamera<width, height>::waitFrame() {
	static const byte frame_pattern[] = "Frame ";
	bool is_found;

	while (true) {
		if (ser.available()) {
			byte incoming_byte = ser.read();

			if (incoming_byte == frame_pattern[0]) {
				is_found = true;
				for (uint32_t i = 1; i < sizeof(frame_pattern) - 1; i++) {
					if (ser.available() && ser.read() != frame_pattern[i]) {
						is_found = false;
						break;
					}
				}

				if (is_found) {
					uint32_t frame_num = 0;

					while (ser.available()) {
						byte next_byte = ser.read();
						if (next_byte >= '0' && next_byte <= '9')
							frame_num = (frame_num << 8) | (next_byte - '0');
					}

					for (uint8_t i = 0; i < 17; i++)
						if (ser.available())
							ser.read();

					return;
				}
			}
		}
	}
}

template <uint16_t width, uint16_t height>
void CustomCamera<width, height>::readLine(byte *_buffer) {
	for (uint16_t bytesRead = 0; bytesRead < width; bytesRead++)
		if (ser.available())
			_buffer[bytesRead++] = ser.read();
}

template <uint16_t width, uint16_t height>
uint32_t CustomCamera<width, height>::getFrameNum() {
	return this->frame_num;
}

template class CustomCamera<CAMERA_WIDTH, CAMERA_HEIGHT>;
