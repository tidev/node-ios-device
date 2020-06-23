#ifndef __DEVICE_INTERFACE_H__
#define __DEVICE_INTERFACE_H__

#include "node-ios-device.h"
#include "mobiledevice.h"
#include <CoreFoundation/CoreFoundation.h>
#include <string>

namespace node_ios_device {

LOG_DEBUG_EXTERN_VARS

enum InterfaceType { USB, WiFi };

/**
 * Represents a specific interface to a device. There are only 2 supported interfaces: USB and
 * Wi-Fi. Whenever something needs to queried or run on the device, it must run through this
 * interface.
 */
class DeviceInterface {
public:
	DeviceInterface(std::string& udid, am_device& dev, uint32_t type);
	~DeviceInterface();

	void connect();
	void disconnect(const bool force = false);
	bool getBoolean(CFStringRef key);
	std::string getString(CFStringRef key);
	void install(std::string& appPath);
	void startService(const char* serviceName, service_conn_t* connection);

	am_device   dev;
	uint32_t    type;

private:
	std::string udid;
	uint32_t    numConnections;
};

}

#endif
