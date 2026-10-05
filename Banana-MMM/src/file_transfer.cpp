#include "file_transfer.h"

#include <string.h>

namespace FileTransfer {

// ---------------------------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------------------------

// Approximate time (in ms) needed to shift `payloadLen` bytes out of the
// CC1101 at the configured bit rate.  Includes preamble + sync-word +
// length + CRC overhead and a 50% safety margin because the GDO-less
// SendData() uses a blind delay() instead of waiting for GDO0.
static uint16_t txDelayFor(uint32_t bitrate, uint8_t payloadLen) {
	// On-air bytes: preamble(~4) + sync(2) + length(1) + payload + CRC(2)
	const uint32_t overheadBytes = 9;
	uint32_t totalBits = (uint32_t)(payloadLen + overheadBytes) * 8UL;
	uint32_t ms = (totalBits * 1000UL) / bitrate;
	if (ms < 5) ms = 5;
	ms = (ms * 3) / 2; // +50% margin
	if (ms > 200) ms = 200;
	return (uint16_t)ms;
}

// ---------------------------------------------------------------------------
// FileSender
// ---------------------------------------------------------------------------

FileSender::FileSender(CustomRadio &radio)
	: _radio(radio), _retries(0), _packets(0) {}

uint16_t FileSender::txDelayFor(uint8_t payloadLen) const {
	return FileTransfer::txDelayFor(_bitrate, payloadLen);
}

bool FileSender::sendPacket(const uint8_t *data, uint8_t len) {
	uint16_t d = txDelayFor(len);
	_radio.SendData((byte *)data, len, d);
	_packets++;
	return true;
}

bool FileSender::waitAck(uint8_t expectedSeq, uint8_t *rxSeq) {
	uint32_t start = millis();
	while ((millis() - start) < ACK_TIMEOUT_MS) {
		if (_radio.CheckRxFifo(0)) {
			if (_radio.CheckCRC()) {
				uint8_t rlen = _radio.ReceiveData(_rxBuf);
				if (rlen >= 2 && _rxBuf[0] == PKT_ACK) {
					if (rxSeq) *rxSeq = _rxBuf[1];
					_radio.SetRx();
					return (_rxBuf[1] == expectedSeq);
				} else if (rlen >= 2 && _rxBuf[0] == PKT_NACK) {
					// Receiver asks us to retransmit the given seq.
					if (rxSeq) *rxSeq = _rxBuf[1];
					_radio.SetRx();
					return false;
				}
				// otherwise stray packet: ignore and keep waiting.
			}
		}
		delay(2);
	}
	_radio.SetRx();
	return false;
}

bool FileSender::sendFile(const char *filename, uint8_t fileId) {
	File f = SD.open(filename, FILE_READ);
	if (!f) {
		return false;
	}
	uint32_t size = f.size();
	bool ok = false;

	// --- Send START packet ---
	uint8_t pkt[MAX_DATA_PKT];
	uint8_t nameLen = strlen(filename);
	if (nameLen > MAX_FILENAME) nameLen = MAX_FILENAME;

	pkt[0] = PKT_START;
	pkt[1] = fileId;
	pkt[2] = (uint8_t)(size >> 0);
	pkt[3] = (uint8_t)(size >> 8);
	pkt[4] = (uint8_t)(size >> 16);
	pkt[5] = (uint8_t)(size >> 24);
	memcpy(pkt + 6, filename, nameLen);
	uint8_t startLen = 6 + nameLen;

	_retries = 0;
	_packets = 0;

	for (uint8_t attempt = 0; attempt < MAX_RETRIES; attempt++) {
		sendPacket(pkt, startLen);
		uint8_t ackSeq = 0xFF;
		if (waitAck(0, &ackSeq) && ackSeq == 0) {
			break;
		}
		_retries++;
		if (attempt + 1 == MAX_RETRIES) {
			f.close();
			return false;
		}
	}

	// --- Send DATA packets ---
	uint32_t offset = 0;
	uint8_t  seq    = 1;
	pkt[0] = PKT_DATA;

	while (offset < size) {
		uint32_t remaining = size - offset;
		uint8_t  chunk     = (remaining > PAYLOAD) ? PAYLOAD : (uint8_t)remaining;

		pkt[1] = seq;
		int n = f.read(pkt + 2, chunk);
		if (n != chunk) {
			f.close();
			return false;
		}

		bool acked = false;
		for (uint8_t attempt = 0; attempt < MAX_RETRIES; attempt++) {
			sendPacket(pkt, 2 + chunk);
			uint8_t ackSeq = 0xFF;
			if (waitAck(seq, &ackSeq)) {
				if (ackSeq == seq) {
					acked = true;
					break;
				}
				// ackSeq points at first missing packet: jump back if needed.
				// (handled below)
			}
			_retries++;
		}
		if (!acked) {
			f.close();
			return false;
		}

		offset += chunk;
		seq++;        // 8-bit wraps at 255, acceptable for stop-and-wait.
	}

	f.close();

	// --- Send END packet ---
	pkt[0] = PKT_END;
	pkt[1] = fileId;
	for (uint8_t attempt = 0; attempt < MAX_RETRIES; attempt++) {
		sendPacket(pkt, 2);
		// After END the receiver doesn't have to ACK (it may close the file);
		// still try a couple of times for reliability.
		delay(50);
		bool done = true;
		// Drain any late ACK.
		if (_radio.CheckRxFifo(0) && _radio.CheckCRC()) {
			uint8_t rlen = _radio.ReceiveData(_rxBuf);
			(void)rlen;
		}
		if (done) break;
	}

	ok = true;
	return ok;
}

bool FileSender::sendBuffer(const char *filename, uint8_t fileId,
                            const uint8_t *data, uint32_t length) {
	// Stream a RAM buffer through the same protocol machinery.
	// Build START.
	uint8_t pkt[MAX_DATA_PKT];
	uint8_t nameLen = strlen(filename);
	if (nameLen > MAX_FILENAME) nameLen = MAX_FILENAME;

	pkt[0] = PKT_START;
	pkt[1] = fileId;
	pkt[2] = (uint8_t)(length >> 0);
	pkt[3] = (uint8_t)(length >> 8);
	pkt[4] = (uint8_t)(length >> 16);
	pkt[5] = (uint8_t)(length >> 24);
	memcpy(pkt + 6, filename, nameLen);
	uint8_t startLen = 6 + nameLen;

	_retries = 0;
	_packets = 0;

	for (uint8_t attempt = 0; attempt < MAX_RETRIES; attempt++) {
		sendPacket(pkt, startLen);
		uint8_t ackSeq = 0xFF;
		if (waitAck(0, &ackSeq) && ackSeq == 0) break;
		_retries++;
		if (attempt + 1 == MAX_RETRIES) return false;
	}

	uint32_t offset = 0;
	uint8_t  seq    = 1;
	pkt[0] = PKT_DATA;

	while (offset < length) {
		uint32_t remaining = length - offset;
		uint8_t  chunk = (remaining > PAYLOAD) ? PAYLOAD : (uint8_t)remaining;

		pkt[1] = seq;
		memcpy(pkt + 2, data + offset, chunk);

		bool acked = false;
		for (uint8_t attempt = 0; attempt < MAX_RETRIES; attempt++) {
			sendPacket(pkt, 2 + chunk);
			uint8_t ackSeq = 0xFF;
			if (waitAck(seq, &ackSeq) && ackSeq == seq) {
				acked = true;
				break;
			}
			_retries++;
		}
		if (!acked) return false;

		offset += chunk;
		seq++;
	}

	pkt[0] = PKT_END;
	pkt[1] = fileId;
	sendPacket(pkt, 2);
	delay(50);
	return true;
}

// ---------------------------------------------------------------------------
// FileReceiver
// ---------------------------------------------------------------------------

FileReceiver::FileReceiver(CustomRadio &rxRadio, CustomRadio &txRadio)
	: _rx(rxRadio), _tx(txRadio),
	  _fileId(0), _fileSize(0), _bytesReceived(0) {
	memset(_rxBuf, 0, sizeof(_rxBuf));
	memset(_filename, 0, sizeof(_filename));
}

uint16_t FileReceiver::txDelayFor(uint8_t payloadLen) const {
	return FileTransfer::txDelayFor(_bitrate, payloadLen);
}

void FileReceiver::flushRx() {
	while (_rx.CheckRxFifo(0)) {
		if (_rx.CheckCRC()) {
			_rx.ReceiveData(_rxBuf);
		} else {
			break;
		}
	}
}

bool FileReceiver::sendAck(uint8_t nextSeq) {
	uint8_t ack[2] = { PKT_ACK, nextSeq };
	_tx.SendData(ack, 2, txDelayFor(2));
	return true;
}

// Returns payload length in _rxBuf, or 0 on timeout/error.
static uint8_t waitForPacket(CustomRadio &rx, uint8_t wantType, uint8_t wantSeq,
                             uint8_t *buf, uint32_t timeoutMs,
                             bool seqCheck) {
	uint32_t start = millis();
	while ((millis() - start) < timeoutMs) {
		if (rx.CheckRxFifo(1)) {
			if (rx.CheckCRC()) {
				uint8_t rlen = rx.ReceiveData(buf);
				if (rlen >= 1 && buf[0] == wantType) {
					if (seqCheck && wantType == PKT_DATA && rlen >= 2 &&
					    buf[1] != wantSeq) {
						// Wrong DATA seq – caller will NACK.
						return 0; // signal wrong sequence; caller handles
					}
					return rlen;
				}
				// else: stray/old packet – ignore and keep listening.
			}
		}
		delay(2);
	}
	return 0;
}

bool FileReceiver::waitPacket(uint8_t wantType, uint8_t wantSeq,
                              uint32_t timeoutMs) {
	return waitForPacket(_rx, wantType, wantSeq, _rxBuf, timeoutMs,
	                     /*seqCheck=*/true) > 0;
}

bool FileReceiver::receiveFile(String &outFilename, uint32_t timeoutMs) {
	_rx.SetRx();
	flushRx();

	// Wait for START.
	uint8_t rlen = waitForPacket(_rx, PKT_START, /*seq*/0, _rxBuf, timeoutMs,
	                             /*seqCheck=*/false);
	if (rlen < 6) {
		return false;
	}

	_fileId       = _rxBuf[1];
	_fileSize     = ((uint32_t)_rxBuf[2])
	              | ((uint32_t)_rxBuf[3] << 8)
	              | ((uint32_t)_rxBuf[4] << 16)
	              | ((uint32_t)_rxBuf[5] << 24);
	uint8_t nameLen = 0;
	for (uint8_t i = 6; i < rlen; i++) {
		if (nameLen >= MAX_FILENAME) break;
		if (_rxBuf[i] == 0) break;
		_filename[nameLen++] = _rxBuf[i];
	}
	_filename[nameLen] = '\0';

	// Ack START (nextSeq = 0 means "start received, ready for seq 1").
	sendAck(0);
	_rx.SetRx();

	// Build the output filename - prefix to avoid overwriting originals.
	String localName = String("rx_") + _filename;
	// FILE_WRITE on Arduino SD creates if missing and truncates; remove first
	// to be safe across different Arduino cores.
	if (SD.exists(localName)) SD.remove(localName);
	File out = SD.open(localName, FILE_WRITE);
	if (!out) {
		return false;
	}

	_bytesReceived = 0;
	uint8_t expectedSeq = 1;

	while (_bytesReceived < _fileSize) {
		rlen = 0;
		bool gotIt = false;
		for (uint8_t attempt = 0; attempt < MAX_RETRIES; attempt++) {
			rlen = waitForPacket(_rx, PKT_DATA, expectedSeq, _rxBuf,
			                     ACK_TIMEOUT_MS + 1500, /*seqCheck=*/true);
			if (rlen >= 3) {
				gotIt = true;
				break;
			}
			// Wrong seq or timeout: send NACK with expected seq and retry.
			uint8_t nack[2] = { PKT_NACK, expectedSeq };
			_tx.SendData(nack, 2, txDelayFor(2));
			_rx.SetRx();
		}
		if (!gotIt || rlen < 3) {
			out.close();
			SD.remove(localName);
			return false;
		}

		uint8_t chunk = rlen - 2; // strip type + seq header
		if (_bytesReceived + chunk > _fileSize) {
			chunk = (uint8_t)(_fileSize - _bytesReceived);
		}
		out.write(_rxBuf + 2, chunk);
		_bytesReceived += chunk;

		sendAck(expectedSeq);
		_rx.SetRx();
		expectedSeq++;
	}

	out.close();

	// Wait for END packet (best effort).
	waitForPacket(_rx, PKT_END, 0, _rxBuf, 500, false);
	sendAck(0xFF);  // final ACK

	outFilename = localName;
	return true;
}

} // namespace FileTransfer
