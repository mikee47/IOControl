#include <IO/Request.h>
#include <IO/Strings.h>

namespace IO
{
#define XX(tag, comment) #tag "\0"
DEFINE_FSTR_LOCAL(IOCOMMAND_STRINGS, IOCOMMAND_MAP(XX))
#undef XX

String toString(Command cmd)
{
	return CStringArray(IOCOMMAND_STRINGS)[unsigned(cmd)];
}

bool fromString(Command& cmd, const char* str)
{
	CStringArray commandStrings(IOCOMMAND_STRINGS);
	auto i = commandStrings.indexOf(str);
	if(i < 0) {
		debug_w("Unknown IO command '%s'", str);
		return false;
	}

	cmd = Command(i);
	return true;
}

Error Request::parseJson(JsonObjectConst json)
{
	Json::getValue(json[FS_id], m_id);

	// Command is optional - may have already been set
	const char* cmd;
	if(Json::getValue(json[FS_command], cmd) && !fromString(m_command, cmd)) {
		return Error::bad_command;
	}

	DevNode node;
	JsonArrayConst arr;
	if(Json::getValue(json[FS_node], node.id)) {
		unsigned count = json[FS_count] | 1;
		while(count--) {
			if(!setNode(node)) {
				return Error::bad_node;
			}
			node.id++;
		}
	} else if(Json::getValue(json[FS_nodes], arr)) {
		for(DevNode::ID id : arr) {
			if(!setNode(DevNode{id})) {
				return Error::bad_node;
			}
		}
	}
	// all nodes
	else if(!setNode(DevNode_ALL)) {
		return Error::bad_node;
	}

	return Error::success;
}

void Request::getJson(JsonObject json) const
{
	if(m_id.length() != 0) {
		json[FS_id] = m_id;
	}
	json[FS_command] = toString(m_command);
	json[FS_device] = m_device.id();
	setStatus(json, m_status);
}

Error Request::submit()
{
	return m_device.submit(this);
}

void Request::handleEvent(Event event)
{
	m_device.handleEvent(this, event);
}

/*
 * Request has completed. Device will destroy Notify originator then destroy this request.
 */
void Request::complete(Status status)
{
	debug_i("Request %p (%s) complete - %s", this, m_id.c_str(), toString(status).c_str());
	m_status = status;
	if(m_callback) {
		m_callback(*this);
	}
	handleEvent(Event::RequestComplete);
}

String Request::caption()
{
	String s(uint32_t(this), HEX);
	s += " (";
	s += m_device.caption();
	s += '/';
	s += m_id;
	s += ')';
	return s;
}

} // namespace IO
