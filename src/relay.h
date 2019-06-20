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

/**
 * A message containing an event and relay message. Instances are created on the background thread,
 * then pushed into the queue where the main thread is notified via libuv to emit the queued
 * messages.
 */
struct RelayMessage {
	RelayMessage(const char* event) : event(event) {}
	RelayMessage(const char* event, std::string& message) : event(event), message(message) {}
	const char* event;
	std::string message;
};

/**
 * A socket connection to a device where incoming data is put in a `RelayMessage` object and queued
 * for emitting.
 *
 * This class contains the list of relay listeners and handles notifying them when new relay
 * messages come in.
 */
class RelayConnection {
public:
	RelayConnection(napi_env env, CFRunLoopRef runloop, CFSocketNativeHandle nativeSocket);
	virtual ~RelayConnection();
	void add(napi_value listener);
	void dispatch();
	void onClose();
	void onData(const char* data);
	void remove(napi_value listener);
	uint32_t size();

protected:
	void connect();
	void disconnect();

	CFSocketNativeHandle      nativeSocket;
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

/**
 * Base class for port and syslog relay classes.
 */
class Relay {
public:
	Relay(napi_env env, Device* device, CFRunLoopRef runloop);
	virtual ~Relay() {};

protected:
	napi_env     env;
	Device*      device;
	CFRunLoopRef runloop;
};

/**
 * Implementation for relaying data from a socket connected to a port on the device.
 *
 * This manages a relay connection for each port. You can still have multiple listeners per port,
 * but that is handled in the `RelayConnection` object.
 */
class PortRelay : public Relay {
public:
	PortRelay(napi_env env, Device* device, CFRunLoopRef runloop);
	void config(RelayAction action, napi_value nport, napi_value listener);

protected:
	std::map<uint32_t, std::shared_ptr<RelayConnection>> connections;
};

/**
 * Implementation for relaying data from the syslog relay service on the device.
 */
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
