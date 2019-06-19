#ifndef __RELAY_H__
#define __RELAY_H__

#include "node-ios-device.h"
#include "device.h"
#include <CoreFoundation/CoreFoundation.h>
#include <list>
#include <map>
#include <mutex>
#include <queue>
#include <uv.h>

namespace node_ios_device {

enum RelayAction { Start, Stop };

class Device;

struct RelayMessage {
	RelayMessage(const char* event) : event(event) {}
	RelayMessage(const char* event, std::string& message) : event(event), message(message) {}
	const char* event;
	std::string message;
};

class RelayConnection {
public:
	RelayConnection(napi_env env, CFRunLoopRef runloop);
	virtual ~RelayConnection();
	void add(napi_value listener, CFSocketNativeHandle sock);
	void dispatch();
	void onClose();
	void onData(const char* data);
	void remove(napi_value listener);
	uint32_t size();

protected:
	void connect(CFSocketNativeHandle sock);
	void disconnect();

	napi_env                  env;
	std::mutex                listenersLock;
	std::list<napi_ref>       listeners;
	CFRunLoopRef              runloop;
	CFSocketRef               socket;
	CFRunLoopSourceRef        source;
	std::queue<RelayMessage*> msgQueue;
	std::mutex                msgQueueLock;
	uv_async_t                msgQueueUpdate;
};

class Relay {
public:
	Relay(napi_env env, Device* device, CFRunLoopRef runloop);
	virtual ~Relay();

protected:
	napi_env     env;
	Device*      device;
	CFRunLoopRef runloop;
};

class PortRelay : public Relay {
public:
	PortRelay(napi_env env, Device* device, CFRunLoopRef runloop);
	void config(RelayAction action, napi_value nport, napi_value listener);

protected:
	std::map<uint32_t, std::shared_ptr<RelayConnection>> connections;
};

class SyslogRelay : public Relay {
public:
	SyslogRelay(napi_env env, Device* device, CFRunLoopRef runloop);
	~SyslogRelay();
	void config(RelayAction action, napi_value listener);

	service_conn_t connection;

protected:
	RelayConnection relayConn;
};

}

#endif
