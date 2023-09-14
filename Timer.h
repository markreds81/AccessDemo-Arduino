#include "esp32-hal.h"
#ifndef _TIMER_h_
#define _TIMER_h_

#if defined(ARDUINO) && ARDUINO >= 100
#include <Arduino.h>
#else
#include <WProgram.h>
#include "pins_arduino.h"
#endif

class Timer {
public:
	Timer() {
		m_millis = 0;
		m_duration = 0;
	};

	virtual ~Timer() { };

	inline void begin(unsigned long duration) {
		m_duration = duration;
		m_millis = millis();
	}

	inline void reset() {
		m_millis = millis();
	}

	inline bool active() {
		return (m_duration > 0);
	}

	inline bool expired() {
		return ((millis() - m_millis) > m_duration);
	}

	inline bool expired(unsigned long duration) {
		return ((millis() - m_millis) > duration);
	}
private:
	unsigned long m_millis;
	unsigned long m_duration;
};

#endif