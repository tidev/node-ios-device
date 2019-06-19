#ifndef __DEVICE_INTERFACE_H__
#define __DEVICE_INTERFACE_H__

#include "node-ios-device.h"
#include "mobiledevice.h"
#include <CoreFoundation/CoreFoundation.h>
#include <string>

namespace node_ios_device {

LOG_DEBUG_EXTERN_VARS

enum InterfaceType { USB, WiFi };

class DeviceInterface {
public:
	DeviceInterface(std::string& udid, am_device& dev);
	~DeviceInterface();

	void connect();
	void disconnect(const bool force = false);
	std::string getProp(CFStringRef key);
	void install(std::string& appPath);
	void startService(const char* serviceName, service_conn_t* connection);

	am_device   dev;

private:
	std::string udid;
	uint32_t    numConnections;
};

}

#endif
