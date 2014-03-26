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
	service_conn_t connection;
	CFSocketRef socket;
	CFRunLoopSourceRef source;
	Listener* logCallback;

	Device(am_device& dev) : device(dev), socket(NULL), source(NULL), logCallback(NULL) {
		X_ASSIGN_PERSISTENT(Object, props, Object::New());
	}

	// fetches info from the device and populates the JavaScript object
	void populate(CFStringRef udid) {
		Local<Object> p = Local<Object>::New(X_ISOLATE_PRE Object::New());

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
				if (AMDeviceConnect(info->dev) == MDERR_OK) {
					if (AMDeviceIsPaired(info->dev) != 1 && AMDevicePair(info->dev) != 1) {
						return;
					}

					if (AMDeviceValidatePairing(info->dev) != MDERR_OK) {
						if (AMDevicePair(info->dev) != 1) {
							return;
						}
						if (AMDeviceValidatePairing(info->dev) != MDERR_OK) {
							return;
						}
					}

					if (AMDeviceStartSession(info->dev) == MDERR_OK) {
						Device* device = new Device(info->dev);
						device->populate(udid);
						CFDictionarySetValue(connected_devices, udid, device);
						devices_changed = true;
					}
				}
			}
			break;

		case ADNCI_MSG_DISCONNECTED:
			udid = AMDeviceCopyDeviceIdentifier(info->dev);
			if (CFDictionaryContainsKey(connected_devices, udid)) {
				// remove the device from the dictionary and destroy it
				Device* device = (Device*)CFDictionaryGetValue(connected_devices, udid);
				CFDictionaryRemoveValue(connected_devices, udid);

				if (device->logCallback) {
					delete device->logCallback;
				}
				if (device->source) {
					CFRelease(device->source);
				}
				if (device->socket) {
					CFRelease(device->socket);
				}

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

	mach_error_t rval;
	CFStringRef keys[] = { CFSTR("PackageType") };
	CFStringRef values[] = { CFSTR("Developer") };
	CFDictionaryRef options = CFDictionaryCreate(NULL, (const void **)&keys, (const void **)&values, 1, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);

	// copy .app to device
	rval = AMDeviceSecureTransferPath(0, *device, localUrl, options, NULL, 0);
	if (rval != MDERR_OK) {
		CFRelease(options);
		CFRelease(localUrl);
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

	X_RETURN_UNDEFINED();
}

/**
 * Handles new data from the socket when listening for a device's syslog messages.
 */
void SocketCallback(CFSocketRef s, CFSocketCallBackType type, CFDataRef address, const void *data, void *info) {
	Device* device = (Device*)info;
	Local<Function> callback = Local<Function>::New(X_ISOLATE_PRE device->logCallback->callback);
	CFIndex length = CFDataGetLength((CFDataRef)data);
	const char *buffer = (const char*)CFDataGetBytePtr((CFDataRef)data);
	char* str = new char[length + 1];
	long i = 0;
	long j = 0;
	char c;
	Handle<Value> argv[1];

	while (length) {
		while (*buffer == '\0') {
			buffer++;
			length--;
			if (length == 0)
				return;
		}

		i = j = 0;

		while (i < length) {
			c = str[j] = buffer[i++];
			if (c == '\n' || c == '\0') {
				str[j] = '\0';
				if (j > 0) {
					argv[0] = String::New(str);
					callback->Call(Context::GetCurrent()->Global(), 1, argv);
				}
				j = 0;
				if (c == '\0') {
					break;
				}
			} else {
				++j;
			}
		}

		length -= i;
		buffer += i;
	}

	delete[] str;
}

/*
 * log()
 * Connects to the device and fires the callback with each line of output from
 * the device's syslog.
 */
X_METHOD(log) {
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

	if (!args[1]->IsFunction()) {
		X_RETURN_VALUE(ThrowException(Exception::Error(String::New("Argument \'callback\' must be a function"))));
	}

	Listener* logCallback = new Listener;
	X_ASSIGN_PERSISTENT(Function, logCallback->callback, Local<Function>::Cast(args[1]));

	Device* deviceObj = (Device*)CFDictionaryGetValue(connected_devices, udidStr);
	CFRelease(udidStr);

	am_device* device = &deviceObj->device;
	mach_error_t rval;
	service_conn_t connection;

	rval = AMDeviceStartService(*device, CFSTR(AMSVC_SYSLOG_RELAY), &connection, NULL);
	if (rval != MDERR_OK) {
		AMDeviceStopSession(*device);
		if (rval == MDERR_SYSCALL) {
			snprintf(tmp, 256, "Failed to start \"%s\" service due to system call error (0x%x)", AMSVC_SYSLOG_RELAY, rval);
		} else if (rval == MDERR_INVALID_ARGUMENT) {
			snprintf(tmp, 256, "Failed to start \"%s\" service due to invalid argument (0x%x)", AMSVC_SYSLOG_RELAY, rval);
		} else {
			snprintf(tmp, 256, "Failed to start \"%s\" service (0x%x)", AMSVC_SYSLOG_RELAY, rval);
		}
		X_RETURN_VALUE(ThrowException(Exception::Error(String::New(tmp))));
	}

	CFSocketContext socketCtx = { 0, device, NULL, NULL, NULL };
	CFSocketRef socket = CFSocketCreateWithNative(kCFAllocatorDefault, connection, kCFSocketDataCallBack, SocketCallback, &socketCtx);
	if (!socket) {
		X_RETURN_VALUE(ThrowException(Exception::Error(String::New("Failed to create socket"))));
	}

	CFRunLoopSourceRef source = CFSocketCreateRunLoopSource(kCFAllocatorDefault, socket, 0);
	if (!source) {
		X_RETURN_VALUE(ThrowException(Exception::Error(String::New("Failed to create socket run loop source"))));
	}

	CFRunLoopAddSource(CFRunLoopGetMain(), source, kCFRunLoopCommonModes);

	if (deviceObj->logCallback) {
		delete deviceObj->logCallback;
	}
	if (deviceObj->source) {
		CFRelease(deviceObj->source);
	}
	if (deviceObj->socket) {
		CFRelease(deviceObj->socket);
	}

	deviceObj->connection = connection;
	deviceObj->socket = socket;
	deviceObj->source = source;
	deviceObj->logCallback = logCallback;

	X_RETURN_UNDEFINED();
}

/*
 * Wire up the JavaScript functions, initialize the dictionaries, and subscribe
 * to the device notifications.
 */
void init(Handle<Object> exports) {
	exports->Set(String::NewSymbol("on"),          FunctionTemplate::New(on)->GetFunction());
	exports->Set(String::NewSymbol("pumpRunLoop"), FunctionTemplate::New(pump_run_loop)->GetFunction());
	exports->Set(String::NewSymbol("devices"),     FunctionTemplate::New(devices)->GetFunction());
	exports->Set(String::NewSymbol("installApp"),  FunctionTemplate::New(installApp)->GetFunction());
	exports->Set(String::NewSymbol("log"),         FunctionTemplate::New(log)->GetFunction());

	listeners = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, NULL);
	connected_devices = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, NULL);

	am_device_notification notification;
	AMDeviceNotificationSubscribe(&on_device_notification, 0, 0, NULL, &notification);
}

#if NODE_MODULE_VERSION > 0x000D
  NODE_MODULE(node_ios_device_v14, init)
#elif NODE_MODULE_VERSION > 0x000C
  NODE_MODULE(node_ios_device_v13, init)
#elif NODE_MODULE_VERSION > 0x000B
  NODE_MODULE(node_ios_device_v12, init)
#elif NODE_MODULE_VERSION > 0x000A
  NODE_MODULE(node_ios_device_v11, init)
#else
  NODE_MODULE(node_ios_device_v1, init)
#endif