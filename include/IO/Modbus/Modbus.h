/*
 * IOModbus.h
 *
 *  Created on: 1 May 2018
 *      Author: mikee47
 *
 *
 * Implements IOControl stack on top of physical interface (modbus.cpp).
 *
 */

#pragma once

#include <IO/Control.h>
#include <IO/Serial.h>
#include "ADU.h"

namespace IO
{
namespace Modbus
{
// Device configuration
DECLARE_FSTR(CONTROLLER_CLASSNAME)

// json tags
DECLARE_FSTR(ATTR_STATES)

class Device;
class Controller;

class Request : public IO::Request
{
	friend Controller;

public:
	Request(Device& device) : IO::Request(reinterpret_cast<IO::Device&>(device))
	{
	}

	const Device& device() const
	{
		return reinterpret_cast<const Device&>(m_device);
	}

	void getJson(JsonObject json) const;

	// Transaction result
	Error error;

protected:
	virtual Function fillRequestData(PDU::Data& data) = 0;
	virtual void callback(PDU& pdu) = 0;

private:
	Exception m_exception = Exception::Success;
};

/*
 * A virtual device, represents a modbus slave device.
 * Actual devices must implement
 *  createRequest()
 *  callback()
 *  fillRequestData()
 */
class Device : public IO::Device
{
public:
	struct SlaveConfig {
		uint8_t address;
		unsigned baudrate;
		unsigned timeout; ///< Max time between command/response
	};

	struct Config : public IO::Device::Config {
		SlaveConfig slave;
	};

	Device(Controller& controller) : IO::Device(reinterpret_cast<IO::Controller&>(controller))
	{
	}

	Error init(const Config& config);
	Error init(JsonObjectConst config) override;

	/**
	 * @brief Handle a broadcast message
	 */
	virtual void onBroadcast(const ADU& adu)
	{
	}

	/**
	 * @brief Handle a message specifically for this device
	 */
	virtual void onRequest(ADU& adu)
	{
	}

	uint16_t address() const override
	{
		return m_config.address;
	}

	unsigned baudrate() const
	{
		return m_config.baudrate;
	}

protected:
	void parseJson(JsonObjectConst json, Config& cfg);

private:
	SlaveConfig m_config;
};

class Controller : public IO::Controller
{
public:
	Controller(Serial& serial, uint8_t instance)
		: IO::Controller(instance), serial{serial}, state{
														.controller{this},
														.onTransmitComplete{transmitComplete},
														.onReceive{receive},
														.rxBufferSize{ADU::MaxSize},
														.txBufferSize{ADU::MaxSize},
													}
	{
	}

	String classname() override
	{
		return CONTROLLER_CLASSNAME;
	}

	void start() override;
	void stop() override;

	bool busy() const override
	{
		return request != nullptr;
	}

	/**
	 * @brief Callback to handle incoming requests
	 * @param adu The request
	 * @retval bool true to sent a response, false to ignore
	 */
	using RequestDelegate = Delegate<bool(ADU& adu)>;

	void onRequest(RequestDelegate callback)
	{
		requestCallback = callback;
	}

protected:
	/**
	 * @brief Override this method to filter or handle incoming requests
	 *
	 * If there is no transaction in progress then unsolicited packets
	 * are interpreted as slave requests.
	 */
	virtual void handleIncomingRequest(ADU& adu);

	void sendResponse(ADU& adu)
	{
		send(adu, adu.prepareResponse());
	}

private:
	void execute(IO::Request& request) override;
	static void IRAM_ATTR transmitComplete(Serial::State& state);
	static void IRAM_ATTR receive(Serial::State& state);

	void send(ADU& adu, size_t size);
	Error readResponse(ADU& adu);
	void completeTransaction();
	void transactionTimeout();

private:
	Serial& serial;
	Serial::State state;
	Request* request{nullptr};
	RequestDelegate requestCallback;
	SimpleTimer timer; ///< Use to schedule callback and timeout
	Function requestFunction{};
};

} // namespace Modbus
} // namespace IO
