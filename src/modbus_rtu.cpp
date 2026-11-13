#include "modbus_rtu.h"

uint16_t modbusCrc(const uint8_t *data, size_t len) {
  uint16_t crc = 0xFFFF;
  for (size_t i = 0; i < len; i++) {
    crc ^= data[i];
    for (int b = 0; b < 8; b++) crc = (crc & 1) ? (crc >> 1) ^ 0xA001 : crc >> 1;
  }
  return crc;
}

void ModbusRtu::begin(Stream *serial, int8_t dePin, uint16_t timeoutMs) {
  _s = serial;
  _de = dePin;
  _timeout = timeoutMs;
  if (_de >= 0) {
    pinMode(_de, OUTPUT);
    digitalWrite(_de, LOW);
  }
}

bool ModbusRtu::request(uint8_t slave, uint8_t fc, uint16_t addr, uint16_t qty) {
  if (_busy || !_s) return false;
  // drop stale bytes (bounded, never loops forever)
  for (int i = 0; i < 512 && _s->available(); i++) _s->read();

  uint8_t f[8] = { slave, fc, (uint8_t)(addr >> 8), (uint8_t)addr, (uint8_t)(qty >> 8), (uint8_t)qty, 0, 0 };
  uint16_t crc = modbusCrc(f, 6);
  f[6] = crc & 0xFF;
  f[7] = crc >> 8;

  if (_de >= 0) digitalWrite(_de, HIGH);
  _s->write(f, sizeof(f));
  _s->flush();   // wait until sent (8 bytes, ~8 ms at 9600 baud)
  if (_de >= 0) digitalWrite(_de, LOW);

  _slave = slave;
  _fc = fc;
  _qty = qty;
  _len = 0;
  _start = millis();
  _busy = true;
  return true;
}

int ModbusRtu::poll() {
  if (!_busy) return MB_OK;

  while (_s->available() && _len < sizeof(_buf)) _buf[_len++] = (uint8_t)_s->read();

  if (_len >= 5) {
    uint16_t expected;
    if (_buf[1] & 0x80) expected = 5;            // exception frame
    else expected = 3 + (uint16_t)_buf[2] + 2;   // read response
    if (_len >= expected) {
      _busy = false;
      if (modbusCrc(_buf, expected - 2) != (uint16_t)(_buf[expected - 2] | (_buf[expected - 1] << 8))) return MB_BAD_CRC;
      if (_buf[0] != _slave) return MB_BAD_SLAVE;
      if ((_buf[1] & 0x7F) != _fc) return MB_BAD_FUNCTION;
      if (_buf[1] & 0x80) return _buf[2] ? _buf[2] : MB_BAD_FUNCTION;
      if (_buf[2] != _qty * 2) return MB_BAD_LENGTH;
      return MB_OK;
    }
  }

  if (millis() - _start > _timeout) {
    _busy = false;
    return MB_TIMEOUT;
  }
  return MB_PENDING;
}

const char *ModbusRtu::errorStr(int code) {
  switch (code) {
    case MB_OK: return "OK";
    case 1: return "Illegal function";
    case 2: return "Illegal data address";
    case 3: return "Illegal data value";
    case 4: return "Slave device failure";
    case MB_TIMEOUT: return "Timeout (no answer)";
    case MB_BAD_CRC: return "CRC error";
    case MB_BAD_SLAVE: return "Wrong slave id";
    case MB_BAD_FUNCTION: return "Wrong function";
    case MB_BAD_LENGTH: return "Wrong length";
    default: return "Error";
  }
}
