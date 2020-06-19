/*
 * Status.h
 *
 *  Created on: 28 May 2018
 *      Author: mikee47
 *
 *
 * In many places JSON request handlers need to report status back.
 * We provide a simple interface to deal with this.
 *
 */

#pragma once

#include "WString.h"
#include <ArduinoJson.h>

namespace IO
{
/*
 * Control status
 */
enum class Status {
	// Request queued
	pending,
	// Request completed
	success,
	// Request failed
	error
};

String toString(Status status);

void setStatus(JsonObject json, Status status);

inline void setSuccess(JsonObject json)
{
	setStatus(json, Status::success);
}

inline void setPending(JsonObject json)
{
	setStatus(json, Status::pending);
}

inline void setError(JsonObject json)
{
	setStatus(json, Status::error);
}

/*
 *
 * @param json
 * @param code    Numeric error code
 * @param text    Text for error
 * @param arg     Argument or additional description
 *
 */
void setError(JsonObject json, int code, const String& text = nullptr, const String& arg = nullptr);

} // namespace IO
