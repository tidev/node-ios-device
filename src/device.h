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

namespace node_ios_device {

LOG_DEBUG_EXTERN_VARS

class PortRelay;

enum DevicePropType { Boolean, String };

/**
 * A variant wrapper for a device property.
 */
class DeviceProp {
public:
	DeviceProp(bool val) : type(Boolean), bval(val) {}
	DeviceProp(std::string val) : type(String), sval(val) {}

	DevicePropType type;
	bool bval;
	std::string sval;
};

/**
 * Contains info for a connected device as well as the interfaces (USB/Wi-Fi) and the relays.
 * Any device-specific queries or execution needs to be run at the interface level.
 */
class Device {
public:
	Device(napi_env env, std::string& udid, am_device& dev, std::weak_ptr<CFRunLoopRef> runloop);

	DeviceInterface* config(am_device& dev, bool isAdd);
	void forward(uint8_t action, napi_value nport, napi_value listener);
	void install(std::string& appPath);
	inline bool isDisconnected() const { return !usb && !wifi; }
	napi_value toJS();

	std::shared_ptr<DeviceInterface> usb;
	std::shared_ptr<DeviceInterface> wifi;

private:
	PortRelay   portRelay;
	napi_env    env;
	std::string udid;
	std::map<const char*, std::unique_ptr<DeviceProp>> props;
};

}

#endif
