#pragma once

#include <IO/Device.h>
#include "Controller.h"

namespace IO
{
namespace RFSwitch
{
DECLARE_FSTR(ATTR_REPEATS)

/**
 * @brief Protocol timings in microseconds
 */
struct Timing {
	uint16_t starth; ///< Width of start High pulse
	uint16_t startl; ///< Width of start Low pulse
	uint16_t period; ///< Bit period
	uint16_t bit0;   ///< Width of a '0' high pulse
	uint16_t bit1;   ///< Width of a '1' high pulse
	uint16_t gap;	///< Gap after final bit before repeating
};

const DeviceClassInfo deviceClass();

/*
 * A specific type of RF device protocol.
 * Actual RF I/O is performed by Controller.
 */
class Device : public IO::Device
{
public:
	struct Config {
		IO::Device::Config base;
		Timing timing;
		uint8_t repeats;
	};

	const DeviceType type() const override
	{
		return DeviceType::RFSwitch;
	}

	Error init(const Config& config);
	Error init(JsonObjectConst config) override;

	Device(Controller& controller) : IO::Device(controller)
	{
	}

	const DeviceClassInfo classInfo() const override
	{
		return deviceClass();
	}

	IO::Request* createRequest() override;

	const Timing& timing() const
	{
		return m_timing;
	}

	uint8_t repeats() const
	{
		return m_repeats;
	}

protected:
	void parseJson(JsonObjectConst json, Config& cfg);

protected:
	Timing m_timing;
	uint8_t m_repeats; ///< Number of times to repeat code
};

} // namespace RFSwitch
} // namespace IO
