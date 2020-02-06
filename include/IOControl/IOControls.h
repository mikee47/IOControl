/*
 * IOControls.h
 *
 *  Created on: 30 May 2018
 *      Author: mikee47
 *
 * Support for simple automation tasks using timers and control (command) chaining.
 * Implements control lists.
 *
 */

#pragma once

#include <ObjectArray.h>
#include "IOControl.h"
#include <core/TimeManager.h>

// Limit on system Timer interval
static constexpr uint32_t IOTIMER_MAX_SECS = 60U * 24U * 90U;
static constexpr uint32_t IOTIMER_MAX_MS = IOTIMER_MAX_SECS * 1000U;
// Returned by CIOTimer::nextCheck() if it doesn't require a check
static constexpr uint32_t IOTIMER_NO_CHECK = IOTIMER_MAX_MS + 1000U;

class DevNode
{
public:
	ioerror_t init(JsonObject json);
	ioerror_t init(const char* devId);
	bool match(const IODevice& device, devnode_id_t nodeId) const;
	devnode_state_t getNodeState() const;

private:
	const IODevice* device_ = nullptr;
	ObjectArray<devnode_id_t> nodes_;
};

class DevNodeArray : public ObjectArray<DevNode>
{
public:
	const DevNode* find(const IODevice& device, devnode_id_t nodeId) const;
};

class IOControl
{
public:
	virtual ~IOControl();

	ioerror_t init(JsonObject json, JsonDocument& deviceCache);

	const String& id() const
	{
		return id_;
	}

	const String& name() const
	{
		return name_ ?: id_;
	}

	const String& data() const
	{
		return data_;
	}

	bool operator==(const IOControl& control) const
	{
		return id_ == control.id_;
	}

	const DevNode* findNode(const IODevice& device, devnode_id_t nodeId) const
	{
		return devnodes_.find(device, nodeId);
	}

	bool contains(const IODevice& device, devnode_id_t nodeId) const
	{
		return devnodes_.find(device, nodeId) != nullptr;
	}

	devnode_state_t getNodeState() const;

	void execute(bool immediate);
	void cancel();

	void requestQueued(const IORequest& request)
	{
		++activeRequests_;
		debug_d("requestQueued(): Control '%s' has %u active requests", id_.c_str(), activeRequests_);
	}

	virtual void requestComplete(const String& requestId);

	uint8_t activeRequests() const
	{
		return activeRequests_;
	}

protected:
	void readDeviceCache(JsonDocument& deviceCache, const String& deviceId, devnode_id_t nodeId);

protected:
	//
	String id_;
	String name_;
	// The json control data
	String data_;
	//
	DevNodeArray devnodes_;
	// Keeps count of active requests for this control
	uint8_t activeRequests_ = 0;
	// Control to execute on completion, if any
	String onComplete_;
	// Set if we're to cancel operations
	bool cancelled_ = false;
};

class IOTimer
{
public:
	IOTimer(const String& id, IOControl& control) : id_(id), control_(control)
	{
	}

	virtual ~IOTimer()
	{
		debug_d("Timer '%s' destroyed", id_.c_str());
	}

	const String& id() const
	{
		return id_;
	}

	const IOControl& control()
	{
		return control_;
	}

	bool operator==(const String& id) const
	{
		return id_ == id;
	}

	bool operator==(const IOTimer& timer) const
	{
		return this == &timer;
	}

	// Get milliseconds to wait until next timer check, max. MAX_TIMER_MS
	virtual unsigned nextCheck(time_t now) const
	{
		return IOTIMER_NO_CHECK;
	}

	virtual void check(time_t now)
	{
	}

	virtual void timeChanged(int adjustSecs)
	{
	}

protected:
	void fire();

protected:
	//
	String id_;
	// Control to invoke when timer fires
	IOControl& control_;
};

class IORelativeTimer : public IOTimer
{
public:
	IORelativeTimer(const String& id, IOControl& control, unsigned delay) : IOTimer(id, control)
	{
		timer_.initializeMs(delay, [](void* arg) { static_cast<IORelativeTimer*>(arg)->fire(); }, this);
		timer_.startOnce();
	}

private:
	//
	SimpleTimer timer_;
};

class IOAbsoluteTimer : public IOTimer
{
public:
	IOAbsoluteTimer(const String& id, IOControl& control, TimeType type, int offset_secs);
	unsigned nextCheck(time_t now) const;
	void check(time_t now);
	void timeChanged(int adjustSecs);

private:
	// See CTimeManager::getTime() for explanation
	//
	TimeType type_;
	//
	int offset_secs_;
	// Keep note of actual time we're using
	time_t time_;
};

// Global list to manage all active timers
class IOTimerList : private Vector<IOTimer>
{
public:
	int indexOf(const String& id) const;
	void add(IOTimer* timer);
	using Vector::removeElement;
	void cancel(const String& id);
	void cancelFor(const IOControl& control);
	void onTimeChange(int adjustSecs);

private:
	unsigned getNextDue();
	void scheduleCheck();
	void check();

private:
	// For checking timers
	SimpleTimer updateTimer_;
};

class IOControlList : public ObjectArray<IOControl>
{
public:
	ioerror_t load(const String& configFile);
	IOControl* find(const String& id);
	bool trigger(const String& id);
	void cancel(const String& id);

private:
	/*
	 * Triggers are handled via task queue, so we use a simple semaphore
	 * to ensure the control object isn't destroyed before the trigger is
	 * handled.
	 */
	uint8_t outstandingTriggers_;
};

extern IOTimerList ioTimers;
extern IOControlList ioControls;
