#pragma once

#include <Platform/Timers.h>

/**
 * @brief Fan controller
 *
 * Output: PWM module output (A or B) used to control fans.
 * Tacho input: PWM-B counts rising edges.
 * Periodically call `readRpm()` which uses pulse count plus elapsed time
 * to calculate and store RPM value. Use `getRpm()` to retrieve that value.
 *
 * Slice   PWM-A 		PWM-B
 *  0       0  16        1  17
 *  1       2  18        3  19
 *  2       4  20        5  21
 *  3       6  22        7  23
 *  4       8            9  
 *  5       10           11
 *  6       12           13
 *  7       14           15
 */
class Fan
{
public:
	Fan(const char* name, uint8_t tachoPin, uint8_t pwmPin) : mName(name), tachoPin(tachoPin), pwmPin(pwmPin)
	{
	}

	bool begin()
	{
		return configureInput() && configureOutput();
	}

	void setLevel(uint8_t percent);
	uint8_t getLevel() const;

	unsigned getRpm() const
	{
		return rpm;
	}

	const char* name() const
	{
		return mName;
	}

	uint16_t readRpm();

private:
	bool configureInput();
	bool configureOutput();

	const char* mName;
	OneShotFastUs us;
	uint8_t tachoPin;
	uint8_t pwmPin;
	uint16_t level;
	uint16_t rpm;
};
