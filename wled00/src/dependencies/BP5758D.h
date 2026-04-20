#pragma once
#ifndef BP5758D_h
#define BP5758D_h

// BP5758D physical channel order mapped to your specific hardware
enum : uint8_t {
  BP_CH_BLUE = 0,   // Channel 1: Blue
  BP_CH_GREEN = 1,  // Channel 2: Green
  BP_CH_RED = 2,    // Channel 3: Red
  BP_CH_WW = 3,     // Channel 4: Warm White
  BP_CH_CW = 4,     // Channel 5: Cold White
  BP_CH_COUNT
};

class BP5758DDriver {
  private:
    static const uint8_t _addrStart5CH = 0xB0;  // 0x80 (Model ID) + 0x30 (5-Channel Start)
    static const uint8_t _addrStandby = 0x80;   // Deep Sleep Command
    static const uint8_t _channelEnable = 0x1F; // 0b00011111 (Enable all 5 channels)

    uint8_t _pinData;
    uint8_t _pinClock;
    uint8_t _currents[BP_CH_COUNT] = {12, 12, 12, 30, 30};
    uint8_t _values[BP_CH_COUNT] = {0, 0, 0, 0, 0};

    inline void sdaLow() {
      pinMode(_pinData, OUTPUT);
      digitalWrite(_pinData, LOW);
    }

    inline void sdaHigh() {
      pinMode(_pinData, INPUT_PULLUP);
    }

    inline void sclLow() {
      pinMode(_pinClock, OUTPUT);
      digitalWrite(_pinClock, LOW);
    }

    inline void sclHigh() {
      pinMode(_pinClock, INPUT_PULLUP);
    }

    inline void i2cDelay() {
      delayMicroseconds(2);
    }

    void startCondition() {
      sdaHigh();
      sclHigh();
      i2cDelay();
      sdaLow();
      i2cDelay();
      sclLow();
    }

    void stopCondition() {
      sdaLow();
      i2cDelay();
      sclHigh();
      i2cDelay();
      sdaHigh();
      i2cDelay();
    }

    void writeByte(uint8_t value) {
      for (uint8_t i = 0; i < 8; i++) {
        sclLow();
        if (value & 0x80) sdaHigh(); else sdaLow();
        i2cDelay();
        sclHigh();
        i2cDelay();
        value <<= 1;
      }
      
      // 9th bit (ACK slot) - Blind Fire to prevent flickering
      sclLow();
      sdaHigh(); 
      i2cDelay();
      sclHigh();
      i2cDelay();
      sclLow();
    }

    // Offset for high currents to prevent register mapping issues
    uint8_t correctCurrent(uint8_t current) {
      if (current < 64) return current;
      return current > 90 ? 90 + 34 : current + 34; 
    }

  public:
    BP5758DDriver(uint8_t pinData, uint8_t pinClock)
      : _pinData(pinData), _pinClock(pinClock) {}

    void begin() {
      sdaHigh();
      sclHigh();
    }

    void setCurrent(uint8_t r, uint8_t g, uint8_t b, uint8_t cw, uint8_t ww) {
      _currents[BP_CH_RED] = correctCurrent(r);
      _currents[BP_CH_GREEN] = correctCurrent(g);
      _currents[BP_CH_BLUE] = correctCurrent(b);
      _currents[BP_CH_CW] = correctCurrent(cw);
      _currents[BP_CH_WW] = correctCurrent(ww);
    }

    inline void setChannel(uint8_t channel, uint8_t value) {
      if (channel < BP_CH_COUNT) _values[channel] = value;
    }

    void update() {
      uint8_t payload[17];
      payload[0] = _addrStart5CH;
      payload[1] = _channelEnable;
      
      // Current limits
      payload[2] = _currents[0]; 
      payload[3] = _currents[1]; 
      payload[4] = _currents[2]; 
      payload[5] = _currents[3]; 
      payload[6] = _currents[4]; 

      // PWM values
      for (uint8_t i = 0; i < BP_CH_COUNT; i++) {
        uint16_t pwm10 = (uint16_t)_values[i] << 2; 
        payload[7 + (i * 2)] = pwm10 & 0x1F;
        payload[8 + (i * 2)] = (pwm10 >> 5) & 0x1F;
      }

      // Blast 17-byte frame
      startCondition();
      for(int i = 0; i < 17; i++) {
        writeByte(payload[i]);
      }
      stopCondition();

      // Deep Sleep Check
      if (_values[0] == 0 && _values[1] == 0 && _values[2] == 0 && _values[3] == 0 && _values[4] == 0) {
        payload[0] = _addrStandby;
        startCondition();
        for(int i = 0; i < 17; i++) {
          writeByte(payload[i]);
        }
        stopCondition();
      }
    }
};

#endif