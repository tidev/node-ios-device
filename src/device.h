/**
 * node-ios-device
 * Copyright (c) 2013-2016 by Appcelerator, Inc. All Rights Reserved.
 * Licensed under the terms of the Apache Public License
 * Please see the LICENSE included with this distribution for details.
 */

#ifndef __DEVICE_H__
#define __DEVICE_H__

#include <v8.h>
#include <boost/format.hpp>
#include <map>
#include "mobiledevice.h"
#include "util.h"

using namespace v8;

namespace node_ios_device {

/**
 * A struct to track listener properties such as the JavaScript callback
 * function.
 */
typedef struct Listener {
	Nan::Persistent<Function> callback;
} Listener;

/**
 * Device object that persists while the device is plugged in. It contains the
 * original MobileDevice device reference and a V8 JavaScript object containing
 * the devices properties.
 */
class Device {
public:
	CFStringRef        udid;
	bool               loaded;
	std::map<std::string, std::string> props;

private:
	am_device          handle;
	int                connected;
//	service_conn_t     logConnection;
	CFSocketRef        logSocket;
	CFRunLoopSourceRef logSource;
	Listener*          logCallback;

public:
	/**
	 * Constructs the device object.
	 */
	Device(am_device& dev) : loaded(false), handle(dev), connected(0), logSocket(NULL), logSource(NULL), logCallback(NULL) {
		this->udid = AMDeviceCopyDeviceIdentifier(dev);

		char* str = cfstring_to_cstr(this->udid);
		if (str == NULL) {
			throw std::runtime_error("Unable to read device UDID");
		}

		this->props["udid"] = std::string(str);
		free(str);
	}

	/**
	 * Disconnects and cleans up allocated memory.
	 */
	~Device() {
		this->disconnect(true);

		if (this->logCallback) {
			delete this->logCallback;
			this->logCallback = NULL;
		}
		if (this->logSource) {
			::CFRelease(this->logSource);
			this->logSource = NULL;
		}
		if (this->logSocket) {
			::CFRelease(this->logSocket);
			this->logSocket = NULL;
		}
	}

	void init() {
		debug("Getting device info: %s", this->props["udid"].c_str());

		// connect to the device and get its information
		this->connect();

		CFNumberRef valueNum = (CFNumberRef)AMDeviceCopyValue(this->handle, 0, CFSTR("HostAttached"));
		int64_t value = 0;
		CFNumberGetValue(valueNum, kCFNumberSInt64Type, &value);
		CFRelease(valueNum);
		if (value != 1) {
			// not physically connected
			debug("Device is not physically connected to host, ignoring");
			return;
		}

		this->set("name",            CFSTR("DeviceName"));
		this->set("buildVersion",    CFSTR("BuildVersion"));
		this->set("cpuArchitecture", CFSTR("CPUArchitecture"));
		this->set("deviceClass",     CFSTR("DeviceClass"));
		this->set("deviceColor",     CFSTR("DeviceColor"));
		this->set("hardwareModel",   CFSTR("HardwareModel"));
		this->set("modelNumber",     CFSTR("ModelNumber"));
		this->set("productType",     CFSTR("ProductType"));
		this->set("productVersion",  CFSTR("ProductVersion"));
		this->set("serialNumber",    CFSTR("SerialNumber"));

		this->loaded = true;
	}

private:
	/**
	 * Connects to the device, pairs with it, and starts a session. We use a
	 * connected counter so that we don't connect more than once.
	 */
	void connect() {
		if (this->connected++ > 0) {
			// already connected
			return;
		}

		// connect to the device
		mach_error_t rval = AMDeviceConnect(this->handle);
		if (rval == MDERR_SYSCALL) {
			throw std::runtime_error("Failed to connect to device: setsockopt() failed");
		} else if (rval == MDERR_QUERY_FAILED) {
			throw std::runtime_error("Failed to connect to device: the daemon query failed");
		} else if (rval == MDERR_INVALID_ARGUMENT) {
			throw std::runtime_error("Failed to connect to device: invalid argument, USBMuxConnectByPort returned 0xffffffff");
		} else if (rval != MDERR_OK) {
			throw std::runtime_error((boost::format("Failed to connect to device (0x%x)") % rval).str());
		}

		// if we're not paired, go ahead and pair now
		if (AMDeviceIsPaired(this->handle) != 1 && AMDevicePair(this->handle) != 1) {
			throw std::runtime_error("Failed to pair device");
		}

		// double check the pairing
		rval = AMDeviceValidatePairing(this->handle);
		if (rval == MDERR_INVALID_ARGUMENT) {
			throw std::runtime_error("Device is not paired: the device is null");
		} else if (rval == MDERR_DICT_NOT_LOADED) {
			throw std::runtime_error("Device is not paired: load_dict() failed");
		} else if (rval != MDERR_OK) {
			throw std::runtime_error((boost::format("Device is not paired (0x%x)") % rval).str());
		}

		// start the session
		rval = AMDeviceStartSession(this->handle);
		if (rval == MDERR_INVALID_ARGUMENT) {
			throw std::runtime_error("Failed to start session: the lockdown connection has not been established");
		} else if (rval == MDERR_DICT_NOT_LOADED) {
			throw std::runtime_error("Failed to start session: load_dict() failed");
		} else if (rval != MDERR_OK) {
			throw std::runtime_error((boost::format("Failed to start session (0x%x)") % rval).str());
		}
	}

	/**
	 * Disconnects the device if there are no other active connections to this
	 * device. Generally, force should not be set. It's mainly there for the
	 * destructor.
	 */
	void disconnect(const bool force = false) {
		if (force || --this->connected <= 0) {
			this->connected = 0;
			AMDeviceStopSession(this->handle);
			AMDeviceDisconnect(this->handle);
		}
	}

	/**
	 * Starts a service.
	 *
	 * Note that if the call to AMDeviceStartService() fails, it's probably
	 * because MobileDevice thinks we're connected and paired, but we're not.
	 */
	void startService(const char* serviceName, service_conn_t* conn) const {
		mach_error_t rval = AMDeviceStartService(this->handle, CFStringCreateWithCStringNoCopy(NULL, serviceName, kCFStringEncodingUTF8, NULL), conn, NULL);
		if (rval == MDERR_SYSCALL) {
			throw std::runtime_error((boost::format("Failed to start \"%s\" service due to system call error (0x%x)") % serviceName % rval).str());
		} else if (rval == MDERR_INVALID_ARGUMENT) {
			throw std::runtime_error((boost::format("Failed to start \"%s\" service due to invalid argument (0x%x)") % serviceName % rval).str());
		} else if (rval != MDERR_OK) {
			throw std::runtime_error((boost::format("Failed to start \"%s\" service (0x%x)") % serviceName % rval).str());
		}
	}

	/**
	 * Sets a property.
	 */
	void set(const char* key, CFStringRef id) {
		CFStringRef valueStr = (CFStringRef)AMDeviceCopyValue(this->handle, 0, id);
		if (valueStr != NULL) {
			char* value = cfstring_to_cstr(valueStr);
			CFRelease(valueStr);
			if (value != NULL) {
				this->props[key] = std::string(value);
				free(value);
			}
		}
	}
};

} // end namespace node_ios_device

#endif
