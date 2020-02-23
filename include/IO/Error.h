#pragma once

#include "WString.h"
#include <ArduinoJson.h>

namespace IO
{
#define IOERROR_MAP(XX)                                                                                                \
	XX(success, "Success")                                                                                             \
	XX(bad_config, "Configuration data invalid")                                                                       \
	XX(access_denied, "Access Denied")                                                                                 \
	XX(file, "File Error")                                                                                             \
	XX(timeout, "Timeout")                                                                                             \
	XX(cancelled, "Cancelled")                                                                                         \
	XX(not_impl, "Not Implemented")                                                                                    \
	XX(bad_controller_class, "Wrong controller class specified for device")                                            \
	XX(bad_controller, "Controller not registered")                                                                    \
	XX(bad_device_class, "Device class not registered")                                                                \
	XX(bad_device, "Device not registered")                                                                            \
	XX(bad_node, "Node ID not valid")                                                                                  \
	XX(bad_command, "Invalid Command")                                                                                 \
	XX(bad_param, "Invalid Parameter")                                                                                 \
	XX(bad_checksum, "Checksum failed")                                                                                \
	XX(bad_size, "Data size invalid")                                                                                  \
	XX(busy, "Device or controller is busy")                                                                           \
	XX(queue_full, "Request queue is full")                                                                            \
	XX(no_mem, "Out of memory")                                                                                        \
	XX(no_config, "No configuration found")                                                                            \
	XX(no_control_id, "Control ID not specified")                                                                      \
	XX(no_device_id, "Device ID not specified")                                                                        \
	XX(no_command, "Command not specified")                                                                            \
	XX(no_address, "Device address not specified")                                                                     \
	XX(no_baudrate, "Device baud rate not specified")                                                                  \
	XX(no_code, "RF code not specified")

enum class Error {
#define XX(tag, comment) tag,
	IOERROR_MAP(XX)
#undef XX
};

inline bool operator!(Error err)
{
	return err == Error::success;
}

String toString(Error err);

#define debug_err(err, arg) debug_w("%s: %s", toString(err).c_str(), (arg).c_str())

Error setError(JsonObject json, Error err, const String& arg = nullptr);

} // namespace IO
