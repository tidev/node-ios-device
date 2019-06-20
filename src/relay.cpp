#include "relay.h"
#include <sstream>

namespace node_ios_device {

RelayConnection::RelayConnection(napi_env env, CFRunLoopRef runloop) :
	env(env),
	runloop(runloop),
	socket(NULL),
	source(NULL) {

	uv_loop_t* loop;
	::napi_get_uv_event_loop(env, &loop);
	msgQueueUpdate.data = this;
	::uv_async_init(loop, &msgQueueUpdate, [](uv_async_t* handle) { ((RelayConnection*)handle->data)->dispatch(); });
	::uv_unref((uv_handle_t*)&msgQueueUpdate);
}

RelayConnection::~RelayConnection() {
	::uv_close((uv_handle_t*)&msgQueueUpdate, NULL);
	disconnect();
}

void RelayConnection::add(napi_value listener, CFSocketNativeHandle sock) {
	napi_ref ref;
	NAPI_THROW_RETURN("RelayConnection::add", "ERROR_NAPI_CREATE_REFERENCE", ::napi_create_reference(env, listener, 1, &ref), )

	size_t count = 0;
	{
		std::lock_guard<std::mutex> lock(listenersLock);
		listeners.push_back(ref);
		count = listeners.size();
	}

	if (count == 1) {
		connect(sock);
		::uv_ref((uv_handle_t*)&msgQueueUpdate);
	}
}

void relaySocketCallback(CFSocketRef s, CFSocketCallBackType type, CFDataRef address, const void* data, void* conn) {
	if (type == kCFSocketDataCallBack) {
		CFDataRef cfdata = (CFDataRef)data;
		CFIndex size = ::CFDataGetLength(cfdata);
		if (size > 0) {
			((RelayConnection*)conn)->onData((const char*)::CFDataGetBytePtr(cfdata));
		} else {
			((RelayConnection*)conn)->onClose();
		}
	}
}

void RelayConnection::connect(CFSocketNativeHandle sock) {
	CFSocketContext socketCtx = { 0, this, NULL, NULL, NULL };

	LOG_DEBUG_1("RelayConnection::connect", "Creating socket using specified file descriptor %d", sock)
	socket = ::CFSocketCreateWithNative(
		kCFAllocatorDefault,
		sock,
		kCFSocketDataCallBack,
		&relaySocketCallback,
		&socketCtx
	);

	if (!socket) {
		throw std::runtime_error("Failed to create socket");
	}

	LOG_DEBUG("RelayConnection::connect", "Creating run loop source")
	source = ::CFSocketCreateRunLoopSource(kCFAllocatorDefault, socket, 0);
	if (!source) {
		throw std::runtime_error("Failed to create socket run loop source");
	}

	LOG_DEBUG("RelayConnection::connect", "Adding socket source to run loop")
	::CFRunLoopAddSource(runloop, source, kCFRunLoopCommonModes);
}

void RelayConnection::disconnect() {
	if (source) {
		LOG_DEBUG("RelayConnection::disconnect", "Removing socket source from run loop")
		::CFRunLoopRemoveSource(runloop, source, kCFRunLoopCommonModes);
		::CFRelease(source);
		source = NULL;
	}

	if (socket) {
		LOG_DEBUG("RelayConnection::disconnect", "Releasing socket")
		::CFSocketInvalidate(socket);
		::CFRelease(socket);
		socket = NULL;
	}
}

void RelayConnection::dispatch() {
	napi_handle_scope scope;
	napi_value global, listener, argv[2], rval;
	int argc;

	NAPI_THROW("RelayConnection::dispatch", "ERR_NAPI_OPEN_HANDLE_SCOPE", ::napi_open_handle_scope(env, &scope))
	NAPI_THROW("RelayConnection::dispatch", "ERR_NAPI_GET_GLOBAL", ::napi_get_global(env, &global))

	std::list<napi_value> callbacks;

	{
		std::lock_guard<std::mutex> lock(listenersLock);
		for (auto const& ref : listeners) {
			NAPI_THROW("RelayConnection::dispatch", "ERR_NAPI_GET_REFERENCE_VALUE", ::napi_get_reference_value(env, ref, &listener))
			if (listener != NULL) {
				callbacks.push_back(listener);
			}
		}
	}

	if (callbacks.empty()) {
		return;
	}

	std::lock_guard<std::mutex> lock2(msgQueueLock);

	while (!msgQueue.empty()) {
		auto relayMsg = msgQueue.front();
		msgQueue.pop();

		argc = 1;
		NAPI_THROW("RelayConnection::dispatch", "ERR_NAPI_CREATE_STRING_UTF8", ::napi_create_string_utf8(env, relayMsg->event, NAPI_AUTO_LENGTH, &argv[0]))

		if (strncmp(relayMsg->event, "end", 3) != 0) {
			argc = 2;
			NAPI_THROW("RelayConnection::dispatch", "ERR_NAPI_CREATE_STRING_UTF8", ::napi_create_string_utf8(env, relayMsg->message.c_str(), NAPI_AUTO_LENGTH, &argv[1]))
		}

		for (auto const& callback : callbacks) {
			NAPI_THROW("RelayConnection::dispatch", "ERR_NAPI_MAKE_CALLBACK", ::napi_make_callback(env, NULL, global, callback, argc, argv, &rval))
		}
	}
}

void RelayConnection::onClose() {
	disconnect();
	{
		std::lock_guard<std::mutex> lock(msgQueueLock);
		msgQueue.push(new RelayMessage("end"));
	}
	::uv_async_send(&msgQueueUpdate);
}

void RelayConnection::onData(const char* data) {
	std::string buffer;
	bool changed = false;

	for (; *data; ++data) {
		if (*data == '\0' || *data == '\r' || *data == '\n') {
			if (!buffer.empty()) {
				std::lock_guard<std::mutex> lock(msgQueueLock);
				msgQueue.push(new RelayMessage("data", buffer));
				changed = true;
				buffer.clear();
			}
		} else {
			buffer += *data;
		}
	}

	if (!buffer.empty()) {
		std::lock_guard<std::mutex> lock(msgQueueLock);
		msgQueue.push(new RelayMessage("data", buffer));
		changed = true;
	}

	if (changed) {
		::uv_async_send(&msgQueueUpdate);
	}
}

void RelayConnection::remove(napi_value listener) {
	std::lock_guard<std::mutex> lock(listenersLock);

	for (auto it = listeners.begin(); it != listeners.end(); ) {
		napi_value callback;
		NAPI_THROW("RelayConnection::remove", "ERR_NAPI_GET_REFERENCE_VALUE", ::napi_get_reference_value(env, *it, &callback))

		bool same;
		NAPI_THROW("RelayConnection::remove", "ERR_NAPI_STRICT_EQUALS", ::napi_strict_equals(env, callback, listener, &same))

		if (same) {
			LOG_DEBUG("RelayConnection::remove", "Removing listener")
			it = listeners.erase(it);
		} else {
			++it;
		}
	}

	if (listeners.size() == 0) {
		::uv_unref((uv_handle_t*)&msgQueueUpdate);
	}
}

uint32_t RelayConnection::size() {
	std::lock_guard<std::mutex> lock(listenersLock);
	return listeners.size();
}

Relay::Relay(napi_env env, Device* device, CFRunLoopRef runloop) :
	env(env),
	device(device),
	runloop(runloop) {}

Relay::~Relay() {}

PortRelay::PortRelay(napi_env env, Device* device, CFRunLoopRef runloop) :
	Relay(env, device, runloop) {}

void PortRelay::config(RelayAction action, napi_value nport, napi_value listener) {
	uint32_t port = 0;
	napi_status status = ::napi_get_value_uint32(env, nport, &port);
	if (status == napi_number_expected || status != napi_ok || port < 1 || port > 65535) {
		throw std::runtime_error("Expected port to be a number between 1 and 65535");
	}

	std::shared_ptr<RelayConnection> conn;
	auto it = connections.find(port);

	if (action == Start) {
		if (it == connections.end()) {
			conn = std::make_shared<RelayConnection>(env, runloop);
			connections.insert(std::make_pair(port, conn));
		} else {
			conn = it->second;
		}

		std::shared_ptr<DeviceInterface> iface = device->usb ? device->usb : device->wifi ? device->wifi : NULL;
		if (!iface) {
			throw std::runtime_error("Device has no USB or Wi-Fi interface");
		}

		uint32_t id = ::AMDeviceGetConnectionID(iface->dev);
		int fd = -1;

		LOG_DEBUG_1("PortRelay::config", "Trying to connect to port %d", port);
		if (::USBMuxConnectByPort(id, htons(port), &fd) != 0) {
			::close(fd);
			std::stringstream error;
			error << "Failed to connect to port " << port;
			throw std::runtime_error(error.str());
		}
		LOG_DEBUG("PortRelay::config", "Connected");

		LOG_DEBUG("PortRelay::config", "Adding listener to port relay connection")
		conn->add(listener, fd);

	} else if (it != connections.end()) {
		LOG_DEBUG("PortRelay::config", "Removing listener from port relay connection")
		it->second->remove(listener);

		if (it->second->size() == 0) {
			LOG_DEBUG("PortRelay::config", "Connection has no more listeners, removing")
			connections.erase(it);
		}
	}
}

SyslogRelay::SyslogRelay(napi_env env, Device* device, CFRunLoopRef runloop) :
	Relay(env, device, runloop),
	relayConn(env, runloop) {}

SyslogRelay::~SyslogRelay() {
	::close(connection);
}

void SyslogRelay::config(RelayAction action, napi_value listener) {
	if (action == Start) {
		if (!device->usb) {
			throw std::runtime_error("syslog requires a USB connected iOS device");
		}
		device->usb->startService(AMSVC_SYSLOG_RELAY, &connection);

		LOG_DEBUG("SyslogRelay::config", "Adding listener to syslog relay connection")
		relayConn.add(listener, connection);

	} else {
		LOG_DEBUG("SyslogRelay::config", "Removing listener from syslog relay connection")
		relayConn.remove(listener);
	}
}

}
