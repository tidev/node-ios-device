/**
 * node-ios-device
 * Copyright (c) 2013-2016 by Appcelerator, Inc. All Rights Reserved.
 * Licensed under the terms of the Apache Public License
 * Please see the LICENSE included with this distribution for details.
 */

#include "runloop.h"
#include "device.h"
#include "mobiledevice.h"
#include "util.h"
#include <CoreFoundation/CoreFoundation.h>
#include <thread>


#include <iostream>


namespace node_ios_device {

CFMutableDictionaryRef connectedDevices = ::CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, NULL);
std::mutex deviceMutex;
static am_device_notification deviceNotification = NULL;

/**
 * The callback when a device notification is received.
 */
void on_device_notification(am_device_notification_callback_info* info, void* arg) {
	bool changed = false;

	if (info->msg == ADNCI_MSG_CONNECTED) {
		std::lock_guard<std::mutex> lock(deviceMutex);
		node_ios_device::Device* device = new node_ios_device::Device(info->dev);

		if (!::CFDictionaryContainsKey(connectedDevices, device->udid)) {
			node_ios_device::debug("Device connected");

			try {
				device->init();

				// if we already have the device info, don't get it again
				if (device->loaded && !::CFDictionaryContainsKey(connectedDevices, device->udid)) {
					::CFDictionarySetValue(connectedDevices, device->udid, device);
					changed = true;
				}
			} catch (...) {
				node_ios_device::debug("Failed to init device");
				delete device;
			}
		}

	} else if (info->msg == ADNCI_MSG_DISCONNECTED) {
		std::lock_guard<std::mutex> lock(deviceMutex);
		CFStringRef udid = ::AMDeviceCopyDeviceIdentifier(info->dev);

		if (::CFDictionaryContainsKey(connectedDevices, udid)) {
			// remove the device from the dictionary and destroy it
			node_ios_device::Device* device = (node_ios_device::Device*)::CFDictionaryGetValue(connectedDevices, udid);
			::CFDictionaryRemoveValue(connectedDevices, udid);

			node_ios_device::debug("Device disconnected: %s", device->props["udid"].c_str());

			delete device;
			changed = true;
		}
	}

	// we need to notify if devices changed and this must be done outside the
	// scopes above so that the mutex is unlocked
	if (changed) {
		node_ios_device::send("devicesChanged");
	}
}

void startRunLoop() {
	node_ios_device::debug("Subscribing to device notifications");
	::AMDeviceNotificationSubscribe(&on_device_notification, 0, 0, NULL, &deviceNotification);

	node_ios_device::debug("Starting CoreFoundation run loop");
	::CFRunLoopRun();
}

void stopRunLoop() {
	::AMDeviceNotificationUnsubscribe(deviceNotification);
	::CFRunLoopStop(::CFRunLoopGetCurrent());
}

// AMDeviceNotificationUnsubscribe(deviceNotification);

} // end namespace node_ios_device
