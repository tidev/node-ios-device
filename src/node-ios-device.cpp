/**
 * node-ios-device
 * Copyright (c) 2013-2016 by Appcelerator, Inc. All Rights Reserved.
 * Licensed under the terms of the Apache Public License
 * Please see the LICENSE included with this distribution for details.
 */

#include <nan.h>
#include <node.h>
#include <v8.h>
#include <uv.h>
#include <chrono>
#include <queue>
#include <thread>
#include "device.h"
#include "message.h"
#include "runloop.h"


#include <iostream>


namespace node_ios_device {
	Nan::Persistent<v8::Object> emitter;
	std::queue<node_ios_device::Message*> msgQueue;
	std::mutex msgQueueMutex;
	uv_async_t dispatchQueueUpdate;
	extern CFMutableDictionaryRef connectedDevices;
	extern std::mutex deviceMutex;
}

/**
 * Async handler when a message has been added to the queue.
 */
static void dispatchQueueCallback(uv_async_t* handle) {
	Nan::HandleScope scope;
	v8::Local<v8::Object> ee = Nan::New(node_ios_device::emitter);

	// check that emitter exists
	if (!ee.IsEmpty()) {
		v8::Local<v8::Function> emit = v8::Local<v8::Function>::Cast(ee->Get(Nan::New("emit").ToLocalChecked()));
		v8::Local<v8::Value> args[2];
		node_ios_device::Message* msg;
		std::queue<node_ios_device::Message*> q;

		{
			// lock the message queue and drain it into a local queue
			std::lock_guard<std::mutex> lock(node_ios_device::msgQueueMutex);
			while (!node_ios_device::msgQueue.empty()) {
				q.push(node_ios_device::msgQueue.front());
				node_ios_device::msgQueue.pop();
			}
		}

		// drain the local queue
		while (!q.empty()) {
			msg = q.front();
			q.pop();
			args[0] = Nan::New(msg->event.c_str()).ToLocalChecked();
			args[1] = Nan::New(msg->data.c_str()).ToLocalChecked();
			delete msg;
			emit->Call(ee, 2, args);
		}
	}
}

/**
 * initialize()
 * Initializes node-ios-device and the event emitter.
 */
NAN_METHOD(initialize) {
	if (info.Length() != 1) {
		return Nan::ThrowError(Exception::Error(Nan::New("Expected 1 argument").ToLocalChecked()));
	}

	if (!info[0]->IsObject()) {
		return Nan::ThrowError(Exception::Error(Nan::New("Argument \'emitter\' must be an object").ToLocalChecked()));
	}

	node_ios_device::emitter.Reset(Local<Object>::Cast(info[0]));
	node_ios_device::debug("Initialized node-ios-device emitter");

	node_ios_device::debug("Starting runloop thread");
	std::thread(node_ios_device::startRunLoop).detach();

	// wait 250ms for the runloop thread to receive initial events
	std::this_thread::sleep_for(std::chrono::milliseconds(250));

	info.GetReturnValue().SetUndefined();
}

/**
 * devices()
 * Defines a JavaScript function that returns a JavaScript array of iOS devices.
 */
NAN_METHOD(devices) {
	if (info.Length() < 1 || !info[0]->IsFunction()) {
		return Nan::ThrowError(Exception::Error(Nan::New("Expected callback to be a function").ToLocalChecked()));
	}

	v8::Local<v8::Function> callback = v8::Local<v8::Function>::Cast(info[0]);
	v8::Local<v8::Array> result = Nan::New<v8::Array>();
	CFIndex size;
	node_ios_device::Device** values;

	{
		std::lock_guard<std::mutex> lock(node_ios_device::deviceMutex);
		size = ::CFDictionaryGetCount(node_ios_device::connectedDevices);
		values = (node_ios_device::Device**)::malloc(size * sizeof(node_ios_device::Device*));
		::CFDictionaryGetKeysAndValues(node_ios_device::connectedDevices, NULL, (const void **)values);
	}

	node_ios_device::debug("Found %d device%s", size, size == 1 ? "" : "s");

	for (CFIndex i = 0; i < size; i++) {
		if (values[i]->loaded) {
			Local<Object> p = Nan::New<Object>();
			for (std::map<std::string, std::string>::iterator it = values[i]->props.begin(); it != values[i]->props.end(); ++it) {
				Nan::Set(p, Nan::New(it->first).ToLocalChecked(), Nan::New(it->second).ToLocalChecked());
			}
			Nan::Set(result, i, p);
		}
	}

	::free(values);

	dispatchQueueCallback(NULL);

	v8::Local<v8::Value> args[] = { Nan::Null(), result };
	callback->Call(Nan::GetCurrentContext()->Global(), 2, args);

	info.GetReturnValue().SetUndefined();
}

/**
 * installApp()
 * Defines a JavaScript function that installs an iOS app on the specified device.
 * /
NAN_METHOD(installApp) {
	char tmp[256];

	if (info.Length() < 2 || info[0]->IsUndefined() || info[1]->IsUndefined()) {
		return Nan::ThrowError(Exception::Error(Nan::New("Missing required arguments \'udid\' and \'appPath\'").ToLocalChecked()));
	}

	// validate the 'udid'
	if (!info[0]->IsString()) {
		return Nan::ThrowError(Exception::Error(Nan::New("Argument \'udid\' must be a string").ToLocalChecked()));
	}

	Handle<String> udidHandle = Handle<String>::Cast(info[0]);
	if (udidHandle->Length() == 0) {
		return Nan::ThrowError(Exception::Error(Nan::New("The \'udid\' must not be an empty string").ToLocalChecked()));
	}

	String::Utf8Value udidValue(udidHandle->ToString());
	char* udid = *udidValue;
	CFStringRef udidStr = CFStringCreateWithCString(NULL, (char*)*udidValue, kCFStringEncodingUTF8);

	if (!CFDictionaryContainsKey(connectedDevices, (const void*)udidStr)) {
		CFRelease(udidStr);
		snprintf(tmp, 256, "Device \'%s\' not connected", udid);
		return Nan::ThrowError(Exception::Error(Nan::New(tmp).ToLocalChecked()));
	}

	Device* deviceObj = (Device*)CFDictionaryGetValue(connectedDevices, udidStr);
	CFRelease(udidStr);
	am_device* device = &deviceObj->handle;

	// validate the 'appPath'
	if (!info[1]->IsString()) {
		return Nan::ThrowError(Exception::Error(Nan::New("Argument \'appPath\' must be a string").ToLocalChecked()));
	}

	Handle<String> appPathHandle = Handle<String>::Cast(info[1]);
	if (appPathHandle->Length() == 0) {
		return Nan::ThrowError(Exception::Error(Nan::New("The \'appPath\' must not be an empty string").ToLocalChecked()));
	}

	String::Utf8Value appPathValue(appPathHandle->ToString());
	char* appPath = *appPathValue;

	// check the file exists
	if (::access(appPath, F_OK) != 0) {
		snprintf(tmp, 256, "The app path \'%s\' does not exist", appPath);
		return Nan::ThrowError(Exception::Error(Nan::New(tmp).ToLocalChecked()));
	}

	// get the path to the app
	CFStringRef appPathStr = CFStringCreateWithCString(NULL, (char*)*appPathValue, kCFStringEncodingUTF8);
	CFURLRef relativeUrl = CFURLCreateWithFileSystemPath(NULL, appPathStr, kCFURLPOSIXPathStyle, false);
	CFURLRef localUrl = CFURLCopyAbsoluteURL(relativeUrl);
	CFRelease(appPathStr);
	CFRelease(relativeUrl);

	try {
		deviceObj->connect();
	} catch (std::runtime_error& e) {
		return Nan::ThrowError(Exception::Error(Nan::New(e.what()).ToLocalChecked()));
	}

	CFStringRef keys[] = { CFSTR("PackageType") };
	CFStringRef values[] = { CFSTR("Developer") };
	CFDictionaryRef options = CFDictionaryCreate(NULL, (const void **)&keys, (const void **)&values, 1, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);

	// copy .app to device
	mach_error_t rval = AMDeviceSecureTransferPath(0, *device, localUrl, options, NULL, 0);
	if (rval != MDERR_OK) {
		AMDeviceStopSession(*device);
		AMDeviceDisconnect(*device);
		deviceObj->connected--;
		CFRelease(options);
		CFRelease(localUrl);
		if (rval == -402653177) {
			return Nan::ThrowError(Exception::Error(Nan::New("Failed to copy app to device: can't install app that contains symlinks").ToLocalChecked()));
		} else {
			snprintf(tmp, 256, "Failed to copy app to device (0x%x)", rval);
			return Nan::ThrowError(Exception::Error(Nan::New(tmp).ToLocalChecked()));
		}
	}

	// install package on device
	rval = AMDeviceSecureInstallApplication(0, *device, localUrl, options, NULL, 0);
	if (rval != MDERR_OK) {
		AMDeviceStopSession(*device);
		AMDeviceDisconnect(*device);
		deviceObj->connected--;
		CFRelease(options);
		CFRelease(localUrl);
		if (rval == -402620395) {
			return Nan::ThrowError(Exception::Error(Nan::New("Failed to install app on device: most likely a provisioning profile issue").ToLocalChecked()));
		} else {
			snprintf(tmp, 256, "Failed to install app on device (0x%x)", rval);
			return Nan::ThrowError(Exception::Error(Nan::New(tmp).ToLocalChecked()));
		}
	}

	// cleanup
	deviceObj->disconnect();
	CFRelease(options);
	CFRelease(localUrl);

	info.GetReturnValue().SetUndefined();
}
*/

/**
 * Handles new data from the socket when listening for a device's syslog messages.
 * /
void LogSocketCallback(CFSocketRef s, CFSocketCallBackType type, CFDataRef address, const void *data, void *info) {
	Device* device = (Device*)info;
	Local<Function> callback = Nan::New<Function>(device->logCallback->callback);
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
					argv[0] = Nan::New(str).ToLocalChecked();
					callback->Call(Nan::GetCurrentContext()->Global(), 1, argv);
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
*/

/**
 * log()
 * Connects to the device and fires the callback with each line of output from
 * the device's syslog.
 * /
NAN_METHOD(log) {
	char tmp[256];

	if (info.Length() < 2 || info[0]->IsUndefined() || info[1]->IsUndefined()) {
		return Nan::ThrowError(Exception::Error(Nan::New("Missing required arguments \'udid\' and \'appPath\'").ToLocalChecked()));
	}

	// validate the 'udid'
	if (!info[0]->IsString()) {
		return Nan::ThrowError(Exception::Error(Nan::New("Argument \'udid\' must be a string").ToLocalChecked()));
	}

	Handle<String> udidHandle = Handle<String>::Cast(info[0]);
	if (udidHandle->Length() == 0) {
		return Nan::ThrowError(Exception::Error(Nan::New("The \'udid\' must not be an empty string").ToLocalChecked()));
	}

	String::Utf8Value udidValue(udidHandle->ToString());
	char* udid = *udidValue;
	CFStringRef udidStr = CFStringCreateWithCString(NULL, (char*)*udidValue, kCFStringEncodingUTF8);

	if (!CFDictionaryContainsKey(node_ios_device::connectedDevices, (const void*)udidStr)) {
		CFRelease(udidStr);
		snprintf(tmp, 256, "Device \'%s\' not connected", udid);
		return Nan::ThrowError(Exception::Error(Nan::New(tmp).ToLocalChecked()));
	}

	if (!info[1]->IsFunction()) {
		return Nan::ThrowError(Exception::Error(Nan::New("Argument \'callback\' must be a function").ToLocalChecked()));
	}

	Listener* logCallback = new Listener;
	logCallback->callback.Reset(Local<Function>::Cast(info[1]));

	Device* deviceObj = (Device*)CFDictionaryGetValue(node_ios_device::connectedDevices, udidStr);
	CFRelease(udidStr);

	// It's possible for the iOS device to not be connected wirelessly instead
	// of with a cable. If that's the case, then AMDeviceStartService() will
	// fail to start the com.apple.syslog_relay service.
	if (!deviceObj->hostConnected) {
		return Nan::ThrowError(Exception::Error(Nan::New("iOS device must be connected to host").ToLocalChecked()));
	}

	service_conn_t connection;

	try {
		deviceObj->connect();
		deviceObj->startService(AMSVC_SYSLOG_RELAY, &connection);
	} catch (std::runtime_error& e) {
		return Nan::ThrowError(Exception::Error(Nan::New(e.what()).ToLocalChecked()));
	}

	deviceObj->disconnect();

	CFSocketContext socketCtx = { 0, deviceObj, NULL, NULL, NULL };
	CFSocketRef socket = CFSocketCreateWithNative(kCFAllocatorDefault, connection, kCFSocketDataCallBack, LogSocketCallback, &socketCtx);
	if (!socket) {
		return Nan::ThrowError(Exception::Error(Nan::New("Failed to create socket").ToLocalChecked()));
	}

	CFRunLoopSourceRef source = CFSocketCreateRunLoopSource(kCFAllocatorDefault, socket, 0);
	if (!source) {
		return Nan::ThrowError(Exception::Error(Nan::New("Failed to create socket run loop source").ToLocalChecked()));
	}

	CFRunLoopAddSource(CFRunLoopGetMain(), source, kCFRunLoopCommonModes);

	if (deviceObj->logCallback) {
		delete deviceObj->logCallback;
	}
	if (deviceObj->logSource) {
		CFRelease(deviceObj->logSource);
	}
	if (deviceObj->logSocket) {
		CFRelease(deviceObj->logSocket);
	}

	deviceObj->logConnection = connection;
	deviceObj->logSocket = socket;
	deviceObj->logSource = source;
	deviceObj->logCallback = logCallback;

	info.GetReturnValue().SetUndefined();
}
*/

/**
 * Called when Node begins to shutdown so that we can clean up any allocated memory.
 */
static void cleanup(void *arg) {
	std::lock_guard<std::mutex> lock(node_ios_device::deviceMutex); // I don't think we really need this

	// free up connected devices
	CFIndex size = ::CFDictionaryGetCount(node_ios_device::connectedDevices);
	CFStringRef* keys = (CFStringRef*)::malloc(size * sizeof(CFStringRef));
	::CFDictionaryGetKeysAndValues(node_ios_device::connectedDevices, (const void **)keys, NULL);
	CFIndex i = 0;

	for (; i < size; i++) {
		node_ios_device::Device* device = (node_ios_device::Device*)::CFDictionaryGetValue(node_ios_device::connectedDevices, keys[i]);
		::CFDictionaryRemoveValue(node_ios_device::connectedDevices, keys[i]);
		delete device;
	}

	::free(keys);

	if (::uv_is_active((uv_handle_t*)&node_ios_device::dispatchQueueUpdate) != 0) {
		::uv_close((uv_handle_t*)&node_ios_device::dispatchQueueUpdate, NULL);
	}

	node_ios_device::stopRunLoop();
}

/**
 * Stops all run loops and releases memory.
 */
NAN_METHOD(shutdown) {
	cleanup(NULL);
}

/**
 * Wire up the JavaScript functions, initialize the dictionaries, and subscribe
 * to the device notifications.
 */
static void init(Handle<Object> exports) {
	::uv_async_init(::uv_default_loop(), &node_ios_device::dispatchQueueUpdate, dispatchQueueCallback);

	exports->Set(Nan::New("initialize").ToLocalChecked(),  Nan::New<FunctionTemplate>(initialize)->GetFunction());
	exports->Set(Nan::New("shutdown").ToLocalChecked(),    Nan::New<FunctionTemplate>(shutdown)->GetFunction());
	exports->Set(Nan::New("devices").ToLocalChecked(),     Nan::New<FunctionTemplate>(devices)->GetFunction());
//	exports->Set(Nan::New("installApp").ToLocalChecked(),  Nan::New<FunctionTemplate>(installApp)->GetFunction());
//	exports->Set(Nan::New("log").ToLocalChecked(),         Nan::New<FunctionTemplate>(log)->GetFunction());

	node::AtExit(cleanup);
}

NODE_MODULE(node_ios_device, init)
