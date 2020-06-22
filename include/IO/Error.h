#pragma once

#include "WString.h"
#include <ArduinoJson.h>

namespace IO
{
using ErrorCode = int16_t;

namespace Error
{
#define IOERROR_MAX_MAP(XX)                                                                                            \
	XX(common, -100)                                                                                                   \
	XX(modbus, -200)                                                                                                   \
	XX(rfswitch, -300)

enum ErrorMax : ErrorCode {
#define XX(name, value) max_##name = value,
	IOERROR_MAX_MAP(XX)
#undef XX
};

#define IOERROR_STD_MAP(XX)                                                                                            \
	XX(access_denied, "Access Denied")                                                                                 \
	XX(timeout, "Timeout")                                                                                             \
	XX(cancelled, "Cancelled")                                                                                         \
	XX(not_impl, "Not Implemented")                                                                                    \
	XX(no_mem, "Out of memory")                                                                                        \
	XX(busy, "Device or controller is busy")                                                                           \
	XX(bad_config, "Configuration data invalid")                                                                       \
	XX(file, "File Error")                                                                                             \
	XX(bad_controller_class, "Wrong controller class specified for device")                                            \
	XX(bad_controller, "Controller not registered")                                                                    \
	XX(bad_device_class, "Device class not registered")                                                                \
	XX(bad_device, "Device not registered")                                                                            \
	XX(bad_node, "Node ID not valid")                                                                                  \
	XX(bad_command, "Invalid Command")                                                                                 \
	XX(bad_param, "Invalid Parameter")                                                                                 \
	XX(bad_checksum, "Checksum failed")                                                                                \
	XX(bad_size, "Data size invalid")                                                                                  \
	XX(queue_full, "Request queue is full")                                                                            \
	XX(no_config, "No configuration found")                                                                            \
	XX(no_control_id, "Control ID not specified")                                                                      \
	XX(no_device_id, "Device ID not specified")                                                                        \
	XX(no_command, "Command not specified")                                                                            \
	XX(no_address, "Device address not specified")                                                                     \
	XX(no_baudrate, "Device baud rate not specified")                                                                  \
	XX(no_code, "RF code not specified")

enum Common : ErrorCode {
	success = 0,
	pending = 1,
	max_common_ = max_common,
#define XX(tag, comment) tag,
	IOERROR_STD_MAP(XX)
#undef XX
};

String toString(ErrorCode err);
} // namespace Error

#define debug_err(err, arg) debug_w("%s: %s", IO::Error::toString(err).c_str(), (arg).c_str())

ErrorCode setSuccess(JsonObject json);

ErrorCode setPending(JsonObject json);

/*
 * @param json
 * @param err	Numeric error code
 * @param text	Text for error
 * @param arg	Argument or additional description
 *
 */
ErrorCode setError(JsonObject json, ErrorCode err, const String& text = nullptr, const String& arg = nullptr);

} // namespace IO
