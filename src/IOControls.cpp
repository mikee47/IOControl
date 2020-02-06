/*
 * IOControls.cpp
 *
 *  Created on: 30 May 2018
 *      Author: mikee47
 *
 */

#include "IOControls.h"
#include <Platform/Timers.h>
#include <SystemClock.h>
#include <Platform/System.h>

// Devices each have separate file for user-customisable information
DEFINE_FSTR_LOCAL(DIR_DEV, "dev/")
DEFINE_FSTR_LOCAL(EXT_JSON, ".json")
DEFINE_FSTR_LOCAL(ATTR_CONTROLS, "controls")
DEFINE_FSTR_LOCAL(ATTR_TIMER, "timer");
DEFINE_FSTR_LOCAL(ATTR_TIME, "time");
DEFINE_FSTR_LOCAL(TIME_SUNRISE, "sunrise")
DEFINE_FSTR_LOCAL(TIME_SUNSET, "sunset")
DEFINE_FSTR_LOCAL(TIME_DUSK, "dusk")
DEFINE_FSTR_LOCAL(TIME_DAWN, "dawn")
DEFINE_FSTR_LOCAL(TIME_NOW, "now")
DEFINE_FSTR_LOCAL(ATTR_OFFSET, "offset")

// Events
DEFINE_FSTR_LOCAL(ATTR_ONCOMPLETE, "oncomplete")
DEFINE_FSTR_LOCAL(ATTR_CANCEL, "cancel")

DEFINE_FSTR_LOCAL(ATTR_INIT, "init")

IOTimerList ioTimers;
IOControlList ioControls;

static TimeType decodeTimeStr(const char* str, int& seconds)
{
	seconds = 0;

	if(str == nullptr)
		return time_invalid;

	if(TIME_NOW == str)
		return time_now;

	if(TIME_SUNRISE == str || TIME_DAWN == str)
		return time_sunrise;

	if(TIME_SUNSET == str || TIME_DUSK == str)
		return time_sunset;

	// hh:nn[:ss]
	char* ptr;
	int secs = strtol(str, &ptr, 10) * SECS_PER_HOUR;
	if(*ptr != ':')
		return time_invalid;
	++ptr;
	secs += strtol(ptr, &ptr, 10) * SECS_PER_MIN;
	if(*ptr == ':') {
		++ptr;
		secs += strtol(ptr, &ptr, 10);
	}
	if(*ptr != '\0')
		return time_invalid;

	seconds = secs;
	return time_absolute;
}

static bool decodeTimeOffset(const char* str, int& seconds)
{
	seconds = 0;

	if(str == nullptr)
		return false;

	// [[hh:]nn:]ss
	char* ptr = (char*)str;
	if(*ptr == '-')
		++ptr;
	int secs = strtol(str, &ptr, 10);
	if(*ptr == ':') {
		++ptr;
		secs = (secs * 60) + strtol(ptr, &ptr, 10);
		if(*ptr == ':') {
			++ptr;
			secs = (secs * 60) + strtol(ptr, &ptr, 10);
		}
	}

	if(*ptr != '\0')
		return false;

	if(*str == '-')
		secs = -secs;
	seconds = secs;
	return true;
}

/* CDevNode */

ioerror_t DevNode::init(JsonObject json)
{
	ioerror_t err = init(json[ATTR_DEVICE].as<const char*>());
	if(err) {
		return err;
	}

	devnode_id_t nodeId;
	JsonArray arr;
	if(Json::getValue(json[ATTR_NODES], arr)) {
		if(!nodes_.init(arr.size())) {
			return ioe_nomem;
		}

		for(unsigned i = 0; i < arr.size(); ++i) {
			nodes_[i] = arr[i];
		}
	} else if(Json::getValue(json[ATTR_NODE], nodeId)) {
		if(!nodes_.init(1)) {
			return ioe_nomem;
		}
		nodes_[0] = nodeId;
	}

	return ioe_success;
}

ioerror_t DevNode::init(const char* devId)
{
	if(devId == nullptr) {
		return ioe_no_device_id;
	}

	device_ = devmgr.findDevice(devId);
	return device_ ? ioe_success : ioe_bad_device;
}

bool DevNode::match(const IODevice& device, devnode_id_t nodeId) const
{
	if(&device != device_) {
		return false;
	}

	if(nodeId == NODES_ALL) {
		return true;
	}

	for(unsigned i = 0; i < nodes_.size(); ++i) {
		if(nodes_[i] == nodeId) {
			return true;
		}
	}

	return false;
}

devnode_state_t DevNode::getNodeState() const
{
	devnode_state_t state = state_none;
	for(unsigned i = 0; i < nodes_.size(); ++i) {
		state |= device_->getNodeState(nodes_[i]);
	}
	return state;
}

/* CDevNodeArray */

const DevNode* DevNodeArray::find(const IODevice& device, devnode_id_t nodeId) const
{
	for(unsigned i = 0; i < size(); ++i) {
		const DevNode& dn = operator[](i);
		if(dn.match(device, nodeId)) {
			return &dn;
		}
	}

	return nullptr;
}

/* CIOControl */

IOControl::~IOControl()
{
	ioTimers.cancelFor(*this);
	debug_d("CIOControl '%s' destroyed", id_.c_str());
}

ioerror_t IOControl::init(JsonObject json, JsonDocument& deviceCache)
{
	const char* devstr = json[ATTR_DEVICE].as<const char*>();
	if(devstr != nullptr) {
		// This will set a default control ID, but that may be overridden (1)
		auto nodeId = json[ATTR_NODE].as<int>();
		readDeviceCache(deviceCache, devstr, nodeId);
	}

	// (1) ID may be overridden
	if(!Json::getValue(json[ATTR_ID], id_) && !id_) {
		return ioe_no_control_id;
	}

	json.remove(ATTR_ID);

	// Name name be overridden
	String attrName = ATTR_NAME;
	if(Json::getValue(json[attrName], name_)) {
		json.remove(attrName);
	}

	if(Json::getValue(json[ATTR_ONCOMPLETE], onComplete_)) {
		json.remove(ATTR_ONCOMPLETE);
	}

	// Store the remaining control information for later execution
	data_ = nullptr;
	data_.reserve(Json::measure(json));
	Json::serialize(json, data_);

	ioerror_t err;
	JsonArray arr;
	if(Json::getValue(json[ATTR_DEVNODES], arr)) {
		if(!devnodes_.init(arr.size())) {
			return ioe_nomem;
		}

		for(unsigned i = 0; i < arr.size(); ++i) {
			err = devnodes_[i].init(arr[i].as<JsonObject>());
			if(err) {
				break;
			}
		}

		return err;
	}

	if(Json::getValue(json[ATTR_DEVICES], arr)) {
		if(!devnodes_.init(arr.size())) {
			return ioe_nomem;
		}

		for(unsigned i = 0; i < arr.size(); ++i) {
			err = devnodes_[i].init(arr[i].as<const char*>());
			if(err) {
				break;
			}
		}

		return err;
	}

	if(json.containsKey(ATTR_DEVICE)) {
		if(!devnodes_.init(1)) {
			return ioe_nomem;
		}

		return devnodes_[0].init(json);
	}

	// Doesn't contain requests
	return ioe_success;
}

void IOControl::readDeviceCache(JsonDocument& deviceCache, const String& deviceId, devnode_id_t nodeId)
{
	// Create default ID string based as <device>.<node>
	id_ = deviceId;
	id_ += '.';
	id_ += nodeId;

	if(!deviceCache.containsKey(deviceId)) {
		IOJsonDocument doc(1024);
		String fn = DIR_DEV;
		fn += deviceId;
		fn += EXT_JSON;
		auto err = doc.loadFromFile(fn);
		if(err) {
			return;
		}
		deviceCache[deviceId] = doc;
	}

	LOAD_FSTR(attrId, ATTR_ID);
	JsonArray nodes = deviceCache[deviceId][ATTR_NODES];
	for(auto node : nodes) {
		if(node[attrId] == nodeId) {
			name_ = node[ATTR_NAME].as<const char*>();
			break;
		}
	}
}

devnode_state_t IOControl::getNodeState() const
{
	devnode_state_t state = state_none;
	for(unsigned i = 0; i < devnodes_.size(); ++i) {
		state |= devnodes_[i].getNodeState();
	}
	return state;
}

/*
 * Called internally under two conditions:
 *
 *  1. Via trigger() method
 *  2. When associated timer fires
 *
 * For (1) if a timer is specified then it is used to defer execution,
 * otherwise we execute immediately.
 *
 * @param immediate   False when control triggered. If a timer is specified then that
 *                    is set up: execute(true) is called when timer fires.
 */
void IOControl::execute(bool immediate)
{
	debug_d("execute(%s, %s)", id_.c_str(), data_.c_str());

	ElapseTimer elapse;

	DynamicJsonDocument doc(1024);
	if(!Json::deserialize(doc, data_)) {
		// We've pre-parsed in init() so shouldn't fail...
		return;
	}

	auto json = doc.as<JsonObject>();

	json[ATTR_ID] = id_;

	JsonArray arr;
	if(Json::getValue(json[ATTR_CANCEL], arr)) {
		for(const char* id : arr)
			ioControls.cancel(id);
	}

	if(!immediate) {
		JsonObject tmrcfg;
		if(Json::getValue(json[ATTR_TIMER], tmrcfg)) {
			// Setup the timer - these are owned by parent control list
			String id = tmrcfg[ATTR_ID];

			ioTimers.cancel(id);

			unsigned delay;
			if(Json::getValue(tmrcfg[ATTR_DELAY], delay)) {
				auto tmr = new IORelativeTimer(id, *this, delay);
				ioTimers.add(tmr);
				return;
			}

			const char* timeStr;
			if(Json::getValue(tmrcfg[ATTR_TIME], timeStr)) {
				int seconds = 0;
				TimeType tt = decodeTimeStr(timeStr, seconds);
				if(tt == time_invalid) {
					// If invalid, ignore this control. Other way is to just run it now.
					debug_w("Invalid time string: '%s'", timeStr);
					return;
				}

				int offset = 0;
				if(Json::getValue(tmrcfg[ATTR_OFFSET], timeStr) && !decodeTimeOffset(timeStr, offset)) {
					debug_w("Invalid time offset: '%s'", timeStr);
					return;
				}

				debug_d("Time offset = %d", offset);

				auto tmr = new IOAbsoluteTimer(id, *this, tt, seconds + offset);
				ioTimers.add(tmr);

				return;
			}

			// We didn't set up a timer, so carry on and execute
		}
	}

	devmgr.handleMessage(nullptr, json, this);

	debug_w("execute(%s) took %s", id_.c_str(), elapse.elapsedTime().toString().c_str());
}

/*
 * Stop any associated timers and do not call onComplete if control is executing.
 */
void IOControl::cancel()
{
	debug_i("CIOControl::cancel() id '%s'", id_.c_str());
	if(activeRequests_ != 0) {
		cancelled_ = true;
	}
	ioTimers.cancelFor(*this);
}

void IOControl::requestComplete(const String& requestId)
{
	--activeRequests_;
	debug_i("requestComplete(): Control '%s' has %u active requests", id_.c_str(), activeRequests_);

	if(activeRequests_ != 0) {
		return;
	}

	if(cancelled_) {
		cancelled_ = false;
	} else if(onComplete_) {
		ioControls.trigger(onComplete_);
	}
}

/* CIOTimer */

void IOTimer::fire()
{
	debug_i("Timer '%s' fired", id_.c_str());
	IOControl& control = control_;
	ioTimers.removeElement(*this);
	control.execute(true);
}

/* CIOAbsoluteTimer */

IOAbsoluteTimer::IOAbsoluteTimer(const String& id, IOControl& control, TimeType type, int offset_secs)
	: IOTimer(id, control), type_(type), offset_secs_(offset_secs)
{
	time_ = timeManager.getTime(type_, offset_secs_);
	debug_i("CIOAbsoluteTimer() '%s' @ %s", id_.c_str(), timeManager.timeStr(time_).c_str());
}

unsigned IOAbsoluteTimer::nextCheck(time_t now) const
{
	// If already expired (shouldn't happen) return a short-ish time
	if(now >= time_) {
		debug_w("Timer '%s' already expired!", id().c_str());
		return 50;
	}

	// Add on a second to allow for timing jitter, etc.
	unsigned secs = 1 + time_ - now;
	debug_i("Timer '%s' due in %u secs", id().c_str(), secs);
	return 1000 * ((secs >= IOTIMER_MAX_SECS) ? IOTIMER_MAX_SECS : secs);
}

void IOAbsoluteTimer::check(time_t now)
{
	debug_i("CIOAbsoluteTimer::check() '%s'", id_.c_str());
	if(now >= time_) {
		fire();
	}
}

void IOAbsoluteTimer::timeChanged(int adjustSecs)
{
	/*
	 * If we asked for 'now' then the previous value requires adjustment.
	 * For other types we need to re-initialise as date may have changed.
	 */
	if(type_ == time_now) {
		time_ += adjustSecs;
	} else {
		time_ = timeManager.getTime(type_, offset_secs_);
	}
}

/* CIOTimerList */

int IOTimerList::indexOf(const String& id) const
{
	for(unsigned i = 0; i < count(); ++i) {
		if(operator[](i) == id) {
			return i;
		}
	}

	return -1;
}

void IOTimerList::onTimeChange(int adjustSecs)
{
	for(unsigned i = 0; i < count(); ++i) {
		operator[](i).timeChanged(adjustSecs);
	}
	scheduleCheck();
}

unsigned IOTimerList::getNextDue()
{
	time_t now = SystemClock.now(eTZ_Local);
	unsigned next = IOTIMER_NO_CHECK;
	for(unsigned i = 0; i < count(); ++i) {
		unsigned ms = operator[](i).nextCheck(now);
		if(ms < next) {
			next = ms;
		}
	}
	return next;
}

void IOTimerList::scheduleCheck()
{
	PSTR_ARRAY(funcName, "IOTimerList::scheduleCheck");

	// Don't act on RTC timers until clock has been set - avoids undefined behaviour
	if(!SystemClock.isSet()) {
		debug_w("%s: clock not yet set", funcName);
		return;
	}

	unsigned due = getNextDue();
	if(due == IOTIMER_NO_CHECK) {
		debug_d("%s: No timers require checking", funcName);
		updateTimer_.stop();
	} else {
		debug_d("%s: Next check in %u ms", funcName, due);
		updateTimer_.initializeMs(due, [](void* arg) { static_cast<IOTimerList*>(arg)->check(); }, this);
		updateTimer_.startOnce();
	}
}

/*
 * Called on timer expiry
 */
void IOTimerList::check()
{
	time_t now = SystemClock.now(eTZ_Local);
	debug_d("IOTimerList::check() at %s", timeManager.timeStr(now).c_str());
	for(unsigned i = 0; i < count(); ++i) {
		operator[](i).check(now);
	}
	scheduleCheck();
}

void IOTimerList::add(IOTimer* timer)
{
	if(timer) {
		cancel(timer->id());
		addElement(timer);
		scheduleCheck();
	}
}

/*
 * Cancel an existing timer, if it exists
 *
 * @param id
 */
void IOTimerList::cancel(const String& id)
{
	int i = indexOf(id);
	if(i >= 0) {
		remove(i);
		debug_i("Timer '%s' cancelled", id.c_str());
		scheduleCheck();
	}
}

void IOTimerList::cancelFor(const IOControl& control)
{
	unsigned i = count();
	while(i-- != 0) {
		if(operator[](i).control() == control) {
			remove(i);
		}
	}
}

/* CIOControlList */

ioerror_t IOControlList::load(const String& configFile)
{
	// Before proceeding, make sure there are no outstanding device requests

	if(outstandingTriggers_ != 0) {
		debug_w("CIOControlList::load - %u outstanding triggers", outstandingTriggers_);
		return ioe_busy;
	}

	unsigned reqCount = 0;
	for(unsigned i = 0; i < size(); ++i) {
		IOControl& c = operator[](i);
		reqCount += c.activeRequests();
	}
	if(reqCount != 0) {
		debug_w("CIOControlList::load - %u active requests", reqCount);
		return ioe_busy;
	}

	IOJsonDocument config(2048);
	auto err = config.loadFromFile(configFile);
	if(err) {
		return err;
	}

	auto arr = config[ATTR_CONTROLS].as<JsonArray>();
	if(!ObjectArray::init(arr.size())) {
		return ioe_nomem;
	}

	DynamicJsonDocument deviceCache(2048);

	for(unsigned i = 0; i < arr.size(); ++i) {
		IOControl& control = operator[](i);
		err = control.init(arr[i].as<JsonObject>(), deviceCache);

		debug_d("control.init('%s') = %d", arr[i]["id"].as<const char*>(), err);

		if(err != ioe_success) {
			debug_err(err, "Control " + control.id() + " init()");
			ObjectArray::free();
			return err;
		}
	}

	trigger(ATTR_INIT);

	return ioe_success;
}

IOControl* IOControlList::find(const String& id)
{
	for(unsigned i = 0; i < size(); ++i) {
		IOControl& control = (*this)[i];
		if(control.id() == id) {
			return &control;
		}
	}
	return nullptr;
}

bool IOControlList::trigger(const String& id)
{
	IOControl* c = find(id);
	if(c == nullptr) {
		return false;
	}

	// Don't call c->execute() directly to avoid risk of deadlocks
	System.queueCallback(
		[](uint32_t param) {
			--ioControls.outstandingTriggers_;
			reinterpret_cast<IOControl*>(param)->execute(false);
		},
		reinterpret_cast<uint32_t>(c));

	++outstandingTriggers_;
	return true;
}

void IOControlList::cancel(const String& id)
{
	IOControl* control = find(id);
	if(control == nullptr) {
		debug_w("cancel(), control '%s' not found", id.c_str());
	} else {
		control->cancel();
	}
}
