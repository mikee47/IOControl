/*
 * IODMX512.cpp
 *
 *  Created on: 1 May 2018
 *      Author: mikee47
 */

#include <IO/DMX512/Request.h>
#include <IO/Strings.h>
#include <SimpleTimer.h>

namespace IO
{
namespace DMX512
{
/*
 * We don't need to use the queue as requests do not perform any I/O.
 * Instead, device state is updated and echoed on next slave update.
 */
ErrorCode Request::submit()
{
	// Only update command gets queued
	if(command() == Command::update) {
		return IO::Request::submit();
	}

	// All others are executed immediately
	auto err = device().execute(*this);
	if(err < 0) {
		debug_e("Request failed, %s", Error::toString(err).c_str());
	}
	complete(err);

	return err;
}

ErrorCode Request::parseJson(JsonObjectConst json)
{
	ErrorCode err = IO::Request::parseJson(json);
	if(err) {
		return err;
	}
	m_value = json[FS_value].as<unsigned>();
	return Error::success;
}

void Request::getJson(JsonObject json) const
{
	IO::Request::getJson(json);
	json[FS_node] = m_node.id;
	json[FS_value] = m_value;
}

bool Request::setNode(DevNode node)
{
	if(!device().isValid(node)) {
		return false;
	}

	m_node = node;
	return true;
}

} // namespace DMX512
} // namespace IO
