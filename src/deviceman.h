#ifndef __DEVICEMAN_H__
#define __DEVICEMAN_H__

#include "node-ios-device.h"
#include "device.h"
#include "mobiledevice.h"
#include <CoreFoundation/CoreFoundation.h>
#include <list>
#include <map>
#include <thread>

namespace node_ios_device {

LOG_DEBUG_EXTERN_VARS

enum WatchAction { Watch, Unwatch };

/**
 * Device Manager that tracks connected devices.
 */
class DeviceMan {
public:
	DeviceMan(napi_env env);
	~DeviceMan();

	void config(napi_value listener, WatchAction action);
	std::shared_ptr<Device> getDevice(std::string& udid);
	void init(WeakPtrWrapper<DeviceMan>* ptr);
	napi_value list();

private:
	void createInitTimer();
	void dispatch();
	void onDeviceNotification(am_device_notification_callback_info* info);
	void run();
	void stopInitTimer();

	WeakPtrWrapper<DeviceMan>* self;
	napi_env env;
	uv_async_t notifyChange;

	std::mutex deviceMutex;
	std::map<std::string, std::shared_ptr<Device>> devices;
	am_device_notification deviceNotification;

	bool initialized;
	CFRunLoopTimerRef initTimer;
	std::timed_mutex initMutex;

	std::shared_ptr<CFRunLoopRef> runloop;

	std::mutex listenersLock;
	std::list<napi_ref> listeners;
};

}

#endif
