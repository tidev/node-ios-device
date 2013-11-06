/**
 * node-ios-device
 * Copyright (c) 2013 by Appcelerator, Inc. All Rights Reserved.
 * Licensed under the terms of the Apache Public License
 * Please see the LICENSE included with this distribution for details.
 */

#include <node.h>
#include <v8.h>
#include <stdlib.h>
#include "mobiledevice.h"

using namespace v8;

/*
 * In Node.js 0.11 and newer, they updated V8 which changed the API. These
 * macros below try to mask the difference between the old and new APIs.
 */
#if NODE_MODULE_VERSION > 0x000B

static v8::Isolate* the_isolate = v8::Isolate::GetCurrent();

#  define X_METHOD(name) void name(const v8::FunctionCallbackInfo<v8::Value>& args)
#  define X_RETURN_UNDEFINED() return
#  define X_RETURN_VALUE(value) return args.GetReturnValue().Set(value)
#  define X_ASSIGN_PERSISTENT(type, handle, obj) handle.Reset(the_isolate, obj)
#  define X_HANDLE_SCOPE(scope) HandleScope scope(the_isolate)
#  define X_ISOLATE_PRE the_isolate,

#else

#  define X_METHOD(name) v8::Handle<v8::Value> name(const v8::Arguments& args)
#  define X_RETURN_UNDEFINED() return v8::Undefined()
#  define X_RETURN_VALUE(value) return value
#  define X_ASSIGN_PERSISTENT(type, handle, obj) handle = v8::Persistent<type>::New(obj)
#  define X_HANDLE_SCOPE(scope) HandleScope scope
#  define X_ISOLATE_PRE

#endif

/*
 * A struct to track listener properties such as the JavaScript callback
 * function.
 */
typedef struct Listener {
	Persistent<Function> callback;
} Listener;

/*
 * Globals
 */
static CFMutableDictionaryRef listeners;
static CFMutableDictionaryRef connected_devices;
static bool devices_changed;

/*
 * Converts CFStringRef strings to C strings.
 */
char* cfstring_to_cstr(CFStringRef str) {
	if (str != NULL) {
		CFIndex length = CFStringGetLength(str);
		CFIndex maxSize = CFStringGetMaximumSizeForEncoding(length, kCFStringEncodingUTF8);
		char* buffer = (char*)malloc(maxSize);
		if (CFStringGetCString(str, buffer, maxSize, kCFStringEncodingUTF8)) {
			return buffer;
		}
	}
	return NULL;
}

/*
 * Device object that persists while the device is plugged in. It contains the
 * original MobileDevice device reference and a V8 JavaScript object containing
 * the devices properties.
 */
class Device {
public:
	am_device device;
	Persistent<Object> props;

	Device(am_device& dev) : device(dev) {
		X_ASSIGN_PERSISTENT(Object, props, Object::New());
	}

	// fetches info from the device and populates the JavaScript object
	void populate(CFStringRef udid) {
		Local<Object> p = Local<Object>::New(Object::New());

		char* str = cfstring_to_cstr(udid);
		if (str != NULL) {
			p->Set(String::NewSymbol("udid"), String::New(str));
			free(str);
		}

		this->getProp(p, "name",            CFSTR("DeviceName"));
		this->getProp(p, "buildVersion",    CFSTR("BuildVersion"));
		this->getProp(p, "cpuArchitecture", CFSTR("CPUArchitecture"));
		this->getProp(p, "deviceClass",     CFSTR("DeviceClass"));
		this->getProp(p, "deviceColor",     CFSTR("DeviceColor"));
		this->getProp(p, "hardwareModel",   CFSTR("HardwareModel"));
		this->getProp(p, "modelNumber",     CFSTR("ModelNumber"));
		this->getProp(p, "productType",     CFSTR("ProductType"));
		this->getProp(p, "productVersion",  CFSTR("ProductVersion"));
		this->getProp(p, "serialNumber",    CFSTR("SerialNumber"));

		X_ASSIGN_PERSISTENT(Object, props, p);
	}

private:
	void getProp(Local<Object>& p, const char* propName, CFStringRef name) {
		CFStringRef value = AMDeviceCopyValue(this->device, 0, name);
		if (value != NULL) {
			char* str = cfstring_to_cstr(value);
			CFRelease(value);
			if (str != NULL) {
				p->Set(String::NewSymbol(propName), String::New(str));
				free(str);
			}
		}
	}
};

/*
 * on()
 * Defines a JavaScript function that adds an event listener.
 */
X_METHOD(on) {
	if (args.Length() >= 2) {
		if (!args[0]->IsString()) {
			X_RETURN_VALUE(ThrowException(Exception::Error(String::New("Argument \'event\' must be a string"))));
		}

		if (!args[1]->IsFunction()) {
			X_RETURN_VALUE(ThrowException(Exception::Error(String::New("Argument \'callback\' must be a function"))));
		}

		Handle<String> event = Handle<String>::Cast(args[0]);
		String::Utf8Value str(event->ToString());
		CFStringRef eventName = CFStringCreateWithCString(NULL, (char*)*str, kCFStringEncodingUTF8);

		Listener* listener = new Listener;
		X_ASSIGN_PERSISTENT(Function, listener->callback, Local<Function>::Cast(args[1]));
		CFDictionarySetValue(listeners, eventName, listener);
	}

	X_RETURN_UNDEFINED();
}

/*
 * Notifies all listeners of an event.
 */
void emit(const char* event) {
	CFStringRef eventStr = CFStringCreateWithCStringNoCopy(NULL, event, kCFStringEncodingUTF8, NULL);
	CFIndex size = CFDictionaryGetCount(listeners);
	CFStringRef* keys = (CFStringRef*)malloc(size * sizeof(CFStringRef));
	CFDictionaryGetKeysAndValues(listeners, (const void **)keys, NULL);
	CFIndex i = 0;

	for (; i < size; i++) {
		if (CFStringCompare(keys[i], eventStr, 0) == kCFCompareEqualTo) {
			const Listener* listener = (const Listener*)CFDictionaryGetValue(listeners, keys[i]);
			if (listener != NULL) {
				Local<Function> callback = Local<Function>::New(X_ISOLATE_PRE listener->callback);
				callback->Call(Context::GetCurrent()->Global(), 0, NULL);
			}
		}
	}

	free(keys);
}

/*
 * pumpRunLoop()
 * Defines a JavaScript function that processes all pending notifications.
 */
X_METHOD(pump_run_loop) {
	CFTimeInterval interval = 0.25;

	if (args.Length() > 0 && args[0]->IsNumber()) {
		Local<Number> intervalArg = Local<Number>::Cast(args[0]);
		interval = intervalArg->NumberValue();
	}

	devices_changed = false;

	CFRunLoopRunInMode(kCFRunLoopDefaultMode, interval, false);

	if (devices_changed) {
		emit("devicesChanged");
	}

	X_RETURN_UNDEFINED();
}

/*
 * devices()
 * Defines a JavaScript function that returns a JavaScript array of iOS devices.
 * This should be called after pumpRunLoop() has been called.
 */
X_METHOD(devices) {
	X_HANDLE_SCOPE(scope);
	Handle<Array> result = Array::New();

	CFIndex size = CFDictionaryGetCount(connected_devices);
	Device** values = (Device**)malloc(size * sizeof(Device*));
	CFDictionaryGetKeysAndValues(connected_devices, NULL, (const void **)values);

	for (CFIndex i = 0; i < size; i++) {
		Persistent<Object>* obj = &values[i]->props;
		result->Set(i, Local<Object>::New(X_ISOLATE_PRE *obj));
	}

	free(values);

	X_RETURN_VALUE(scope.Close(result));
}

/*
 * The callback when a device notification is received.
 */
void on_device_notification(am_device_notification_callback_info* info, void* arg) {
	CFStringRef udid;

	switch (info->msg) {
		case ADNCI_MSG_CONNECTED:
			udid = AMDeviceCopyDeviceIdentifier(info->dev);
			if (!CFDictionaryContainsKey(connected_devices, udid)) {
				// connect to the device and get its information
				AMDeviceConnect(info->dev);
				if (AMDeviceIsPaired(info->dev) && AMDeviceValidatePairing(info->dev) == MDERR_OK && AMDeviceStartSession(info->dev) == MDERR_OK) {
					Device* device = new Device(info->dev);
					device->populate(udid);
					CFDictionarySetValue(connected_devices, udid, device);
					AMDeviceStopSession(info->dev);
					devices_changed = true;
				}
				AMDeviceDisconnect(info->dev);
			}
			break;

		case ADNCI_MSG_DISCONNECTED:
			udid = AMDeviceCopyDeviceIdentifier(info->dev);
			if (CFDictionaryContainsKey(connected_devices, udid)) {
				// remove the device from the dictionary and destroy it
				const Device* device = (const Device*)CFDictionaryGetValue(connected_devices, udid);
				CFDictionaryRemoveValue(connected_devices, udid);
				delete device;
				devices_changed = true;
			}
			break;
	}
}

/*
 * installApp()
 * Defines a JavaScript function that installs an iOS app on the specified device.
 * This should be called after pumpRunLoop() has been called.
 */
X_METHOD(installApp) {
	char tmp[256];

	if (args.Length() < 2 || args[0]->IsUndefined() || args[1]->IsUndefined()) {
		X_RETURN_VALUE(ThrowException(Exception::Error(String::New("Missing required arguments \'udid\' and \'appPath\'"))));
	}

	// validate the 'udid'
	if (!args[0]->IsString()) {
		X_RETURN_VALUE(ThrowException(Exception::Error(String::New("Argument \'udid\' must be a string"))));
	}

	Handle<String> udidHandle = Handle<String>::Cast(args[0]);
	if (udidHandle->Length() == 0) {
		X_RETURN_VALUE(ThrowException(Exception::Error(String::New("The \'udid\' must not be an empty string"))));
	}

	String::Utf8Value udidValue(udidHandle->ToString());
	char* udid = *udidValue;
	CFStringRef udidStr = CFStringCreateWithCString(NULL, (char*)*udidValue, kCFStringEncodingUTF8);

	if (!CFDictionaryContainsKey(connected_devices, (const void*)udidStr)) {
		CFRelease(udidStr);
		snprintf(tmp, 256, "Device \'%s\' not connected", udid);
		X_RETURN_VALUE(ThrowException(Exception::Error(String::New(tmp))));
	}

	Device* deviceObj = (Device*)CFDictionaryGetValue(connected_devices, udidStr);
	CFRelease(udidStr);
	am_device* device = &deviceObj->device;

	// validate the 'appPath'
	if (!args[1]->IsString()) {
		X_RETURN_VALUE(ThrowException(Exception::Error(String::New("Argument \'appPath\' must be a string"))));
	}

	Handle<String> appPathHandle = Handle<String>::Cast(args[1]);
	if (appPathHandle->Length() == 0) {
		X_RETURN_VALUE(ThrowException(Exception::Error(String::New("The \'appPath\' must not be an empty string"))));
	}

	String::Utf8Value appPathValue(appPathHandle->ToString());
	char* appPath = *appPathValue;

	// check the file exists
	if (access(appPath, F_OK) != 0) {
		snprintf(tmp, 256, "The app path \'%s\' does not exist", appPath);
		X_RETURN_VALUE(ThrowException(Exception::Error(String::New(tmp))));
	}

	// get the path to the app
	CFStringRef appPathStr = CFStringCreateWithCString(NULL, (char*)*appPathValue, kCFStringEncodingUTF8);
	CFURLRef relativeUrl = CFURLCreateWithFileSystemPath(NULL, appPathStr, kCFURLPOSIXPathStyle, false);
	CFURLRef localUrl = CFURLCopyAbsoluteURL(relativeUrl);
	CFRelease(appPathStr);
	CFRelease(relativeUrl);

	// connect to the device
	mach_error_t rval = AMDeviceConnect(*device);
	if (rval == MDERR_SYSCALL) {
		X_RETURN_VALUE(ThrowException(Exception::Error(String::New("Failed to connect to device: setsockopt() failed"))));
	} else if (rval == MDERR_QUERY_FAILED) {
		X_RETURN_VALUE(ThrowException(Exception::Error(String::New("Failed to connect to device: the daemon query failed"))));
	} else if (rval == MDERR_INVALID_ARGUMENT) {
		X_RETURN_VALUE(ThrowException(Exception::Error(String::New("Failed to connect to device: invalid argument, USBMuxConnectByPort returned 0xffffffff"))));
	} else if (rval != MDERR_OK) {
		snprintf(tmp, 256, "Failed to connect to device (0x%x)", rval);
		X_RETURN_VALUE(ThrowException(Exception::Error(String::New(tmp))));
	}

	// make sure we're paired
	rval = AMDeviceIsPaired(*device);
	if (rval != 1) {
		rval = AMDevicePair(*device);
		if (rval != 1) {
			X_RETURN_VALUE(ThrowException(Exception::Error(String::New("Device is not paired"))));
		}
	}

	// double check the pairing
	rval = AMDeviceValidatePairing(*device);
	if (rval != MDERR_OK) {
		rval = AMDevicePair(*device);
		if (rval != 1) {
			X_RETURN_VALUE(ThrowException(Exception::Error(String::New("Failed to pair device"))));
		} else {
			rval = AMDeviceValidatePairing(*device);
			if (rval == MDERR_INVALID_ARGUMENT) {
				X_RETURN_VALUE(ThrowException(Exception::Error(String::New("Device is not paired: the device is null"))));
			} else if (rval == MDERR_DICT_NOT_LOADED) {
				X_RETURN_VALUE(ThrowException(Exception::Error(String::New("Device is not paired: load_dict() failed"))));
			} else if (rval != MDERR_OK) {
				snprintf(tmp, 256, "Device is not paired (0x%x)", rval);
				X_RETURN_VALUE(ThrowException(Exception::Error(String::New(tmp))));
			}
		}
	}

	// start the session
	rval = AMDeviceStartSession(*device);
	if (rval == MDERR_INVALID_ARGUMENT) {
		X_RETURN_VALUE(ThrowException(Exception::Error(String::New("Failed to start session: the lockdown connection has not been established"))));
	} else if (rval == MDERR_DICT_NOT_LOADED) {
		X_RETURN_VALUE(ThrowException(Exception::Error(String::New("Failed to start session: load_dict() failed"))));
	} else if (rval != MDERR_OK) {
		snprintf(tmp, 256, "Failed to start session (0x%x)", rval);
		X_RETURN_VALUE(ThrowException(Exception::Error(String::New(tmp))));
	}

	CFStringRef keys[] = { CFSTR("PackageType") };
	CFStringRef values[] = { CFSTR("Developer") };
	CFDictionaryRef options = CFDictionaryCreate(NULL, (const void **)&keys, (const void **)&values, 1, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);

	// copy .app to device
	rval = AMDeviceSecureTransferPath(0, *device, localUrl, options, NULL, 0);
	if (rval != MDERR_OK) {
		CFRelease(options);
		CFRelease(localUrl);
		AMDeviceDisconnect(*device);
		if (rval == -402653177) {
			X_RETURN_VALUE(ThrowException(Exception::Error(String::New("Failed to copy app to device: can't install app that contains symlinks"))));
		} else {
			snprintf(tmp, 256, "Failed to copy app to device (0x%x)", rval);
			X_RETURN_VALUE(ThrowException(Exception::Error(String::New(tmp))));
		}
	}

	// install package on device
	rval = AMDeviceSecureInstallApplication(0, *device, localUrl, options, NULL, 0);
	if (rval != MDERR_OK) {
		CFRelease(options);
		CFRelease(localUrl);
		AMDeviceDisconnect(*device);
		if (rval == -402620395) {
			X_RETURN_VALUE(ThrowException(Exception::Error(String::New("Failed to install app on device: most likely a provisioning profile issue"))));
		} else {
			snprintf(tmp, 256, "Failed to install app on device (0x%x)", rval);
			X_RETURN_VALUE(ThrowException(Exception::Error(String::New(tmp))));
		}
	}

	// cleanup
	CFRelease(options);
	CFRelease(localUrl);
	AMDeviceDisconnect(*device);

	X_RETURN_UNDEFINED();
}

/*
 * Wire up the JavaScript functions, initialize the dictionaries, and subscribe
 * to the device notifications.
 */
void init(Handle<Object> exports) {
	exports->Set(String::NewSymbol("on"), FunctionTemplate::New(on)->GetFunction());
	exports->Set(String::NewSymbol("pumpRunLoop"), FunctionTemplate::New(pump_run_loop)->GetFunction());
	exports->Set(String::NewSymbol("devices"), FunctionTemplate::New(devices)->GetFunction());
	exports->Set(String::NewSymbol("installApp"), FunctionTemplate::New(installApp)->GetFunction());

	listeners = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, NULL);
	connected_devices = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, NULL);

	am_device_notification notification;
	AMDeviceNotificationSubscribe(&on_device_notification, 0, 0, NULL, &notification);
}

#if NODE_MODULE_VERSION > 0x000C
  NODE_MODULE(node_ios_device_v13, init)
#elif NODE_MODULE_VERSION > 0x000B
  NODE_MODULE(node_ios_device_v12, init)
#elif NODE_MODULE_VERSION > 0x000A
  NODE_MODULE(node_ios_device_v11, init)
#else
  NODE_MODULE(node_ios_device_v1, init)
#endif