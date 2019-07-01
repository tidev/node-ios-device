#include "relay.h"
#include <sstream>

namespace node_ios_device {

/**
 * Initializes the relay connection and wires up the relay message async handler into Node's libuv
 * runloop.
 */
RelayConnection::RelayConnection(napi_env env, std::weak_ptr<CFRunLoopRef> runloop, int* fd) :
	fd(fd),
	env(env),
	runloop(runloop),
	socket(NULL),
	source(NULL) {}

/**
 * Shuts down a relay connection.
 */
RelayConnection::~RelayConnection() {
	::uv_close((uv_handle_t*)&msgQueueUpdate, NULL);
	disconnect();
}

/**
 * Adds a listener. If this is the first listener, it increments/refs the libuv async handle so
 * prevent Node from exiting.
 */
void RelayConnection::add(napi_value listener) {
	napi_ref ref;
	NAPI_THROW_RETURN("RelayConnection::add", "ERROR_NAPI_CREATE_REFERENCE", ::napi_create_reference(env, listener, 1, &ref), )

	size_t count = 0;
	{
		std::lock_guard<std::mutex> lock(listenersLock);
		listeners.push_back(ref);
		count = listeners.size();
	}

	if (count == 1) {
		::uv_ref((uv_handle_t*)&msgQueueUpdate);
		connect();
	}
}

/**
 * Dispatches activity from the relay socket back to the relay connection object.
 */
static void relaySocketCallback(CFSocketRef s, CFSocketCallBackType type, CFDataRef address, const void* data, void* connData) {
	if (type == kCFSocketDataCallBack) {
		CFDataRef cfdata = (CFDataRef)data;
		CFIndex size = ::CFDataGetLength(cfdata);

		std::shared_ptr<RelayConnection>* conn = static_cast<std::shared_ptr<RelayConnection>*>(connData);
		if (size > 0) {
			(*conn)->onData((const char*)::CFDataGetBytePtr(cfdata));
		} else {
			(*conn)->onClose();
		}
	}
}

/**
 * Connects to the specified native socket and wires up the callback.
 */
void RelayConnection::connect() {
	CFSocketContext socketCtx = { 0, &self, NULL, NULL, NULL };

	LOG_DEBUG_1("RelayConnection::connect", "Creating socket using specified file descriptor %d", *fd)
	socket = ::CFSocketCreateWithNative(
		kCFAllocatorDefault,
		(CFSocketNativeHandle)*fd,
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
	if (auto rl = runloop.lock()) {
		::CFRunLoopAddSource(*rl, source, kCFRunLoopCommonModes);
	}
}

/**
 * Creates an shared pointer to an instance of the device.
 */
std::shared_ptr<RelayConnection> RelayConnection::create(napi_env env, std::weak_ptr<CFRunLoopRef> runloop, int* fd) {
	std::shared_ptr<RelayConnection> conn = std::make_shared<RelayConnection>(env, runloop, fd);
	conn->init();
	return conn;
}

/**
 * Disconnects the socket and stops listening for incoming data.
 */
void RelayConnection::disconnect() {
	if (source) {
		LOG_DEBUG("RelayConnection::disconnect", "Removing socket source from run loop")
		if (auto rl = runloop.lock()) {
			::CFRunLoopRemoveSource(*rl, source, kCFRunLoopCommonModes);
		}
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

/**
 * Notifies all relay connection listeners of new data or the connection ending.
 */
void RelayConnection::dispatch() {
	napi_handle_scope scope;
	napi_value global, listener, argv[2], rval;
	int argc;

	NAPI_THROW("RelayConnection::dispatch", "ERR_NAPI_OPEN_HANDLE_SCOPE", ::napi_open_handle_scope(env, &scope))
	NAPI_THROW("RelayConnection::dispatch", "ERR_NAPI_GET_GLOBAL", ::napi_get_global(env, &global))

	std::list<napi_value> callbacks;

	// resolves the listener N-API references and caches the JS callback functions in the
	// `callbacks` list allowing the listener list to be quickly unlocked
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

	while (1) {
		std::shared_ptr<RelayMessage> relayMsg;

		{
			std::lock_guard<std::mutex> lock(msgQueueLock);

			// flush the relay connection data to the listeners
			if (msgQueue.empty()) {
				break;
			}

			relayMsg = msgQueue.front();
			msgQueue.pop();
		}

		bool isEnd = strncmp(relayMsg->event, "end", 3) == 0;

		argc = 1;
		NAPI_THROW("RelayConnection::dispatch", "ERR_NAPI_CREATE_STRING_UTF8", ::napi_create_string_utf8(env, relayMsg->event, NAPI_AUTO_LENGTH, &argv[0]))

		if (isEnd) {
			LOG_DEBUG_1("RelayConnection::dispatch", "Emitting \"%s\" event", relayMsg->event);
		} else {
			argc = 2;
			NAPI_THROW("RelayConnection::dispatch", "ERR_NAPI_CREATE_STRING_UTF8", ::napi_create_string_utf8(env, relayMsg->message.c_str(), NAPI_AUTO_LENGTH, &argv[1]))
		}

		for (auto const& callback : callbacks) {
			NAPI_THROW("RelayConnection::dispatch", "ERR_NAPI_MAKE_CALLBACK", ::napi_make_callback(env, NULL, global, callback, argc, argv, &rval))

			if (isEnd) {
				remove(callback);
			}
		}

		if (isEnd) {
			disconnect();
			return;
		}
	}
}

/**
 * Explicit initialization so that we can get a weak pointer based on the shared pointer that
 * created this instance and wire up the libuv callback.
 */
void RelayConnection::init() {
	self = shared_from_this();

	uv_loop_t* loop;
	::napi_get_uv_event_loop(env, &loop);
	msgQueueUpdate.data = &self;
	::uv_async_init(loop, &msgQueueUpdate, [](uv_async_t* handle) {
		std::weak_ptr<RelayConnection>* ptr = static_cast<std::weak_ptr<RelayConnection>*>(handle->data);
		if (auto conn = (*ptr).lock()) {
			conn->dispatch();
		}
	});
	::uv_unref((uv_handle_t*)&msgQueueUpdate);
}

/**
 * Creates an "end" message and queues it.
 */
void RelayConnection::onClose() {
	{
		std::lock_guard<std::mutex> lock(msgQueueLock);
		msgQueue.push(std::make_shared<RelayMessage>("end"));
	}
	::uv_async_send(&msgQueueUpdate);
}

/**
 * Creates an "data" message for each line and queues it.
 */
void RelayConnection::onData(const char* data) {
	std::string buffer;
	bool changed = false;

	{
		std::lock_guard<std::mutex> lock(msgQueueLock);

		for (; *data; ++data) {
			if (*data == '\0' || *data == '\r' || *data == '\n') {
				if (!buffer.empty()) {
					msgQueue.push(std::make_shared<RelayMessage>("data", buffer));
					changed = true;
					buffer.clear();
				}
			} else {
				buffer += *data;
			}
		}

		if (!buffer.empty()) {
			msgQueue.push(std::make_shared<RelayMessage>("data", buffer));
			changed = true;
		}
	}

	if (changed) {
		::uv_async_send(&msgQueueUpdate);
	}
}

/**
 * Removes a callback from the relay connection. Once there are no more listeners, it
 * decrements/unrefs the libuv async handle to all Node to exit.
 */
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
		disconnect();
	}
}

/**
 * Returns the number of listeners for this relay connection.
 */
uint32_t RelayConnection::size() {
	std::lock_guard<std::mutex> lock(listenersLock);
	return listeners.size();
}

/**
 * Initializes the base relay instance.
 */
Relay::Relay(napi_env env, std::weak_ptr<CFRunLoopRef> runloop) :
	env(env),
	runloop(runloop) {}

/**
 * Intializes a port relay instance along with its base class.
 */
PortRelay::PortRelay(napi_env env, std::weak_ptr<CFRunLoopRef> runloop) :
	Relay(env, runloop) {}

/**
 * Adds or removes a listener to the specified port's relay connection.
 */
void PortRelay::config(uint8_t action, napi_value nport, napi_value listener, std::shared_ptr<DeviceInterface> iface) {
	uint32_t port = 0;
	napi_status status = ::napi_get_value_uint32(env, nport, &port);
	if (status == napi_number_expected || status != napi_ok || port < 1 || port > 65535) {
		throw std::runtime_error("Expected port to be a number between 1 and 65535");
	}

	std::shared_ptr<RelayConnection> conn;
	auto it = connections.find(port);

	if (action == RELAY_START) {
		if (it == connections.end()) {
			// port relay connection does not exist, so create it
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

			conn = RelayConnection::create(env, runloop, &fd);
			connections.insert(std::make_pair(port, conn));
		} else {
			conn = it->second;
		}

		LOG_DEBUG("PortRelay::config", "Adding listener to port relay connection")
		conn->add(listener);

	} else if (it != connections.end()) {
		LOG_DEBUG("PortRelay::config", "Removing listener from port relay connection")
		it->second->remove(listener);

		if (it->second->size() == 0) {
			LOG_DEBUG("PortRelay::config", "Connection has no more listeners, removing")
			connections.erase(it);
		}
	}
}

/**
 * Intializes a syslog relay instance along with its base class.
 */
SyslogRelay::SyslogRelay(napi_env env, std::weak_ptr<CFRunLoopRef> runloop) :
	Relay(env, runloop) {

	relayConn = RelayConnection::create(env, runloop, (int*)&connection);
}

/**
 * Closes the connection handle.
 */
SyslogRelay::~SyslogRelay() {
	::close(connection);
}

/**
 * Adds or removes a listener to the syslog relay connection.
 */
void SyslogRelay::config(uint8_t action, napi_value listener, std::shared_ptr<DeviceInterface> iface) {
	if (action == RELAY_START) {
		iface->startService(AMSVC_SYSLOG_RELAY, &connection);

		LOG_DEBUG("SyslogRelay::config", "Adding listener to syslog relay connection")
		relayConn->add(listener);

	} else {
		LOG_DEBUG("SyslogRelay::config", "Removing listener from syslog relay connection")
		relayConn->remove(listener);
	}
}

}
