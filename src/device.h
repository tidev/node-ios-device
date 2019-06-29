#ifndef __DEVICE_H__
#define __DEVICE_H__

#include "node-ios-device.h"
#include "device-interface.h"
#include "mobiledevice.h"
#include "relay.h"
#include <CoreFoundation/CoreFoundation.h>
#include <list>
#include <map>
#include <string>
#include <variant>

namespace node_ios_device {

LOG_DEBUG_EXTERN_VARS

class PortRelay;
class SyslogRelay;

enum DevicePropType { Boolean, String };

/**
 * A variant wrapper for a device property.
 */
class DeviceProp {
public:
	DeviceProp(std::string val) : type(String) {
		value = val;
	}
	DeviceProp(bool val) : type(Boolean) {
		value = val;
	}

	DevicePropType type;
	std::variant<bool, std::string> value;
};

/**
 * Contains info for a connected device as well as the interfaces (USB/Wi-Fi) and the relays.
 * Any device-specific queries or execution needs to be run at the interface level.
 */
class Device {
public:
	Device(napi_env env, std::string& udid, am_device& dev, std::weak_ptr<CFRunLoopRef> runloop);
	virtual ~Device() {}

	DeviceInterface* config(am_device& dev, bool isAdd);
	void forward(uint8_t action, napi_value nport, napi_value listener);
	void install(std::string& appPath);
	inline bool isDisconnected() const { return !usb && !wifi; }
	void syslog(uint8_t action, napi_value listener);
	napi_value toJS();

	std::shared_ptr<DeviceInterface> usb;
	std::shared_ptr<DeviceInterface> wifi;

private:
	PortRelay   portRelay;
	SyslogRelay syslogRelay;
	napi_env    env;
	std::string udid;
	std::map<const char*, std::unique_ptr<DeviceProp>> props;
};

}

#endif
