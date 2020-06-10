#pragma once

#include <IO/Device.h>
#include "Controller.h"

namespace IO
{
namespace RFSwitch
{
DECLARE_FSTR(ATTR_REPEATS)

/**
 * @brief Protocol configuration
 */
struct Protocol {
	/// Timings in microseconds
	struct {
		uint16_t starth; ///< Width of start High pulse
		uint16_t startl; ///< Width of start Low pulse
		uint16_t period; ///< Bit period
		uint16_t bit0;   ///< Width of a '0' high pulse
		uint16_t bit1;   ///< Width of a '1' high pulse
		uint16_t gap;	///< Gap after final bit before repeating
	} timing;
	uint8_t repeats; ///< Number of times to repeat code
};

/*
 * A specific type of RF device protocol.
 * Actual RF I/O is performed by Controller.
 */
class Device : public IO::Device
{
public:
	Device(Controller& controller) : IO::Device(controller)
	{
	}

	const DeviceClassInfo classInfo() const override;

	const DeviceType type() const override
	{
		return DeviceType::RFSwitch;
	}

	IO::Request* createRequest() override;

	const Protocol& protocol() const
	{
		return m_protocol;
	}

protected:
	Error init(JsonObjectConst config) override;

protected:
	Protocol m_protocol;
};

} // namespace RFSwitch
} // namespace IO
