#pragma once

#include <WString.h>
#include "Controller.h"
#include "Request.h"
#include <ArduinoJson.h>

namespace IO
{
class Request;

/*
 * A request goes through the following states:
 *  submitted - request.submit()
 *  queued    - Controller places request on internal queue
 *  executed  - Controller retrieves request from queue
 *  completed - Request fully handled, status indicates success/failure
 *
 * An Controller invokes this callback twice, when a request is about to
 * be executed and again when it has completed.
 * Call `request.isPending()` to determine which.
 */
//using IoCallback = Delegate<void(Request& request)>;

using ControllerMap = HashMap<String, Controller*>;

class DeviceManager
{
public:
	/**
	 * @brief Controllers register themselves so they can be located
	 * @note we don't own the controller; these are typically static objects
	 */
	void registerController(Controller& controller);

	/**
	 * @brief Device classes call this to register themselves
	 */
	void registerDeviceClass(GetDeviceClass devclass);

	Controller* findController(const String& name) const
	{
		return m_controllers[name];
	}

	/**
	 * @brief Load device config and create device tree.
	 */
	ErrorCode begin(JsonObjectConst config);

	ErrorCode end();

	void start();

	ErrorCode stop();

	bool canStop();

	Device* findDevice(const String& id);

	ErrorCode createRequest(const String& devid, Request*& request);

	/*
	 * Generally, requests should fit into the general model so that IO::Request can be used.
	 * If more custom behviour is required then this method can be used to up-cast to the appropriate
	 * Request object.
	 * NOTE: At present there is no type checking on this, so use care.
	 */
	template <class RequestClass> ErrorCode createRequest(const String& devid, RequestClass*& request)
	{
		Request* req;
		auto err = createRequest(devid, req);
		if(!err) {
			request = reinterpret_cast<RequestClass*>(req);
		} else {
			debug_w("createRequest('%s'): %s", devid.c_str(), Error::toString(err).c_str());
		}
		return err;
	}

	/**
	 * @brief set the callback handler function for all I/O requests
	 * @note Callback invoked twice; once when executed, then again when completed.
	 */
	void setCallback(Request::Callback callback)
	{
		m_callback = callback;
	}

	/**
	 * @brief invoke the callback, if one is registered
	 * @note called by Controller
	 */
	void callback(Request& request)
	{
		if(m_callback) {
			m_callback(request);
		}
	}

	ErrorCode handleMessage(JsonObject json, Request::Callback callback);

private:
	ControllerMap m_controllers; ///< We don't own the controllers
	Request::Callback m_callback;
};

extern DeviceManager devmgr;

} // namespace IO
