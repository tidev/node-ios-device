#include "node-ios-device.h"
#include "deviceman.h"

namespace node_ios_device {
	std::shared_ptr<DeviceMan> deviceman = NULL;
	napi_ref logRef = NULL;
	LOG_DEBUG_VARS
}

using namespace node_ios_device;

/**
 * Flushes the debug log message queue. This can be called at anytime from the main thread. As soon
 * as new log messages are added to the queue, `dispatchLog()` is notified, but because it's async,
 * it's possible that a sync operation will complete before Node's runloop will come around and
 * process pending notifications, so it's encouraged to manually flush the log messages before a
 * public API function returns.
 */
void flushLog(napi_env env) {
#ifndef ENABLE_RAW_DEBUGGING
	napi_handle_scope scope;
	napi_value global, logFn, rval;

	NAPI_FATAL("dispatchLog", napi_open_handle_scope(env, &scope))

	if (!logRef) {
		return;
	}

	NAPI_FATAL("dispatchLog", napi_get_reference_value(env, logRef, &logFn))

	if (logFn == NULL) {
		return;
	}

	NAPI_FATAL("dispatchLog", napi_get_global(env, &global))

	std::lock_guard<std::mutex> lock(logLock);

	while (!logQueue.empty()) {
		std::shared_ptr<LogMessage> obj = logQueue.front();
		logQueue.pop();
		napi_value argv[2];

		if (obj->ns.length()) {
			NAPI_FATAL("dispatchLog", napi_create_string_utf8(env, obj->ns.c_str(), obj->ns.length(), &argv[0]))
		} else {
			NAPI_FATAL("dispatchLog", napi_get_null(env, &argv[0]))
		}
		NAPI_FATAL("dispatchLog", napi_create_string_utf8(env, obj->msg.c_str(), obj->msg.length(), &argv[1]))

		// we have to create an async context to prevent domain.enter error
		napi_value resName;
		NAPI_FATAL("dispatchLog", napi_create_string_utf8(env, "node_ios_device.log", NAPI_AUTO_LENGTH, &resName))
		napi_async_context ctx;
		NAPI_FATAL("dispatchLog", napi_async_init(env, NULL, resName, &ctx))

		// emit the log message
		napi_status status = napi_make_callback(env, ctx, global, logFn, 2, argv, &rval);

		napi_async_destroy(env, ctx);

		NAPI_FATAL("dispatchLog", status)
	}

	NAPI_FATAL("dispatchLog", napi_close_handle_scope(env, scope))
#endif
}

#ifndef ENABLE_RAW_DEBUGGING
/**
 * Called when new log messages are added to the queue. This function runs on the main thread and
 * is fired by libuv. Since it's called async, it's possible for Node to exit before processing
 * any pending log notifications and you should explicitly call `flushLog()`.
 */
void dispatchLog(uv_async_t* handle) {
	flushLog((napi_env)handle->data);
}
#endif

/**
 * init()
 * Wires up debug log message notification handler, prints the node-ios-device banner, and
 * initializes the device manager and it's background thread runloop.
 */
NAPI_METHOD(init) {
#ifndef ENABLE_RAW_DEBUGGING
	NAPI_ARGV(1);
	napi_value logFn = argv[0];

	// wire up the log notification handler
	uv_loop_t* loop;
	napi_get_uv_event_loop(env, &loop);
	logNotify.data = env;
	uv_async_init(loop, &logNotify, &dispatchLog);
	uv_unref((uv_handle_t*)&logNotify);

	// create the reference for the emit log callback so it doesn't get GC'd
	NAPI_THROW_RETURN("init", "ERR_NAPI_CREATE_REFERENCE", napi_create_reference(env, logFn, 1, &logRef), NULL)

	// print the banner
	napi_value global, result, args[2];
	uint32_t apiVersion;
	char banner[128];
	const napi_node_version* ver;
	NAPI_THROW_RETURN("init", "ERR_NAPI_GET_GLOBAL", napi_get_global(env, &global), NULL)
	NAPI_THROW_RETURN("init", "ERR_NAPI_GET_NULL", napi_get_null(env, &args[0]), NULL)
	NAPI_THROW_RETURN("init", "ERR_NAPI_GET_VERSION", napi_get_version(env, &apiVersion), NULL)
	NAPI_THROW_RETURN("init", "ERR_NAPI_GET_NODE_VERSION", napi_get_node_version(env, &ver), NULL)
	snprintf(banner, 128, "v" NODE_IOS_DEVICE_VERSION " <" NODE_IOS_DEVICE_URL "> (%s %d.%d.%d/n-api %d)", ver->release, ver->major, ver->minor, ver->patch, apiVersion);
	NAPI_THROW_RETURN("init", "ERR_NAPI_CREATE_STRING", napi_create_string_utf8(env, banner, strlen(banner), &args[1]), NULL)
	NAPI_THROW_RETURN("init", "ERR_NAPI_CALL_FUNCTION", napi_call_function(env, global, logFn, 2, args, &result), NULL)
#endif

	deviceman->init(new WeakPtrWrapper<DeviceMan>(deviceman));

	NAPI_RETURN_UNDEFINED("init")
}

/**
 * Helper function that converts a JavaScript string into a std string.
 */
std::string napi_string_to_std_string(napi_env env, napi_value str) {
	size_t len;
	std::string rval;

	NAPI_THROW_RETURN("napi_string_to_std_string", "ERR_NAPI_GET_VALUE_STRING", napi_get_value_string_utf8(env, str, NULL, 0, &len), NULL)
	rval.reserve(len + 1);
	rval.resize(len);
	NAPI_THROW_RETURN("napi_string_to_std_string", "ERR_NAPI_GET_VALUE_STRING", napi_get_value_string_utf8(env, str, &rval[0], rval.capacity(), NULL), NULL)

	return rval;
}

/**
 * install()
 * Installs an app to the specified iOS device.
 */
NAPI_METHOD(install) {
	NAPI_ARGV(2);

	try {
		std::string udid = napi_string_to_std_string(env, argv[0]);
		std::shared_ptr<Device> device = deviceman->getDevice(udid);
		std::string appPath = napi_string_to_std_string(env, argv[1]);
		device->install(appPath);
	} catch (std::exception& e) {
		const char* msg = e.what();
		LOG_DEBUG_1("DeviceMan::install", "%s", msg)
		NAPI_THROW_ERROR("ERR_INSTALL", msg, ::strlen(msg), NULL)
	}

	flushLog(env);
	NAPI_RETURN_UNDEFINED("install")
}

/**
 * list()
 * Retrieves a list all connected iOS devices.
 */
NAPI_METHOD(list) {
	napi_value rval = deviceman->list();
	flushLog(env);
	return rval;
}

/**
 * Helper for generating the forward() and syslog() functions.
 */
#define CREATE_LOG_METHOD(name, argc, errCode, code) \
	NAPI_METHOD(name) { \
		NAPI_ARGV(argc); \
		try { \
			std::string udid = napi_string_to_std_string(env, argv[0]); \
			std::shared_ptr<Device> device = deviceman->getDevice(udid); \
			code; \
		} catch (std::exception& e) { \
			const char* msg = e.what(); \
			LOG_DEBUG_1(STRINGIFY(name), "Error: %s", msg) \
			NAPI_THROW_ERROR(errCode, msg, ::strlen(msg), NULL) \
		} \
		flushLog(env); \
		NAPI_RETURN_UNDEFINED(STRINGIFY(name)) \
	}

/**
 * forward() and syslog()
 * All of the logic is performed in the device's relay object.
 */
CREATE_LOG_METHOD(startForward, 3, "ERR_FORWARD_START", device->portRelay->config(Start, argv[1], argv[2]))
CREATE_LOG_METHOD(stopForward,  3, "ERR_FORWARD_STOP",  device->portRelay->config(Stop, argv[1], argv[2]))

CREATE_LOG_METHOD(startSyslog,  2, "ERR_SYSLOG_START",  device->syslogRelay->config(Start, argv[1]))
CREATE_LOG_METHOD(stopSyslog,   2, "ERR_SYSLOG_STOP",   device->syslogRelay->config(Stop, argv[1]))

/**
 * watch()
 * Starts watching for connected devices.
 */
NAPI_METHOD(watch) {
	NAPI_ARGV(1);
	deviceman->config(argv[0], node_ios_device::Watch);
	flushLog(env);
	NAPI_RETURN_UNDEFINED("watch")
}

/**
 * unwatch()
 * Stops watching devices.
 */
NAPI_METHOD(unwatch) {
	NAPI_ARGV(1);
	deviceman->config(argv[0], node_ios_device::Unwatch);
	flushLog(env);
	NAPI_RETURN_UNDEFINED("watch")
}

/**
 * Destroys the Watchman instance and closes open handles.
 */
static void cleanup(void* arg) {
	if (deviceman) {
		LOG_DEBUG("cleanup", "Deleting deviceman")
		deviceman.reset();
	}

#ifndef ENABLE_RAW_DEBUGGING
	uv_close((uv_handle_t*)&logNotify, NULL);

	if (logRef) {
		napi_delete_reference((napi_env)arg, logRef);
		logRef = NULL;
	}
#endif
}

/**
 * Wire up the public API and cleanup handler and creates the Watchman instance.
 */
NAPI_INIT() {
	NAPI_EXPORT_FUNCTION(init);
	NAPI_EXPORT_FUNCTION(install);
	NAPI_EXPORT_FUNCTION(list);
	NAPI_EXPORT_FUNCTION(startForward);
	NAPI_EXPORT_FUNCTION(startSyslog);
	NAPI_EXPORT_FUNCTION(stopForward);
	NAPI_EXPORT_FUNCTION(stopSyslog);
	NAPI_EXPORT_FUNCTION(watch);
	NAPI_EXPORT_FUNCTION(unwatch);

	NAPI_THROW("napi_init", "ERR_NAPI_ADD_ENV_CLEANUP_HOOK", napi_add_env_cleanup_hook(env, cleanup, env))

	deviceman = std::make_shared<DeviceMan>(env);
}
