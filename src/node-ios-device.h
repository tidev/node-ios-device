#ifndef __NODE_IOS_DEVICE_H__
#define __NODE_IOS_DEVICE_H__

// enable the following line to bypass the message queue and print the raw debug log messages to stdout
// #define ENABLE_RAW_DEBUGGING

#define NAPI_VERSION 3

#include <memory>
#include <mutex>
#include <napi-macros.h>
#include <node_api.h>
#include <queue>
#include <thread>
#include <uv.h>

namespace node_ios_device {
	/**
	 * A debug log message including the namespace.
	 */
	struct LogMessage {
		LogMessage() {}
		LogMessage(const std::string ns, const std::string msg) : ns(ns), msg(msg) {}
		std::string ns;
		std::string msg;
	};
}

#define STRINGIFY(s) STRINGIFY_HELPER(s)
#define STRINGIFY_HELPER(s) #s

#define NAPI_THROW_ERROR(code, msg, len, rval) \
	{ \
		napi_value err, errCode, message; \
		NAPI_STATUS_THROWS(::napi_create_string_utf8(env, code, NAPI_AUTO_LENGTH, &errCode)) \
		NAPI_STATUS_THROWS(::napi_create_string_utf8(env, msg, len, &message)) \
		::napi_create_error(env, errCode, message, &err); \
		::napi_throw(env, err); \
		return rval; \
	}

#define NAPI_THROW_RETURN(ns, code, call, rval) \
	{ \
		napi_status _status = call; \
		if (_status != napi_ok) { \
			const napi_extended_error_info* error; \
			::napi_get_last_error_info(env, &error); \
			\
			char msg[1024]; \
			::snprintf(msg, 1024, "%s: " #call " failed (status=%d) %s", ns, _status, error->error_message); \
			\
			napi_value err, errCode, message; \
			NAPI_STATUS_THROWS(::napi_create_string_utf8(env, code, NAPI_AUTO_LENGTH, &errCode)) \
			NAPI_STATUS_THROWS(::napi_create_string_utf8(env, msg, ::strlen(msg), &message)) \
			\
			::napi_create_error(env, errCode, message, &err); \
			::napi_throw(env, err); \
			return rval; \
		} \
	}

#define NAPI_THROW(ns, code, call) NAPI_THROW_RETURN(ns, code, call, )

#define NAPI_RETURN_UNDEFINED(ns) \
	{ \
		napi_value undef; \
		NAPI_THROW_RETURN(ns, "ERR_NAPI_GET_UNDEFINED", ::napi_get_undefined(env, &undef), NULL) \
		return undef; \
	}

#define NAPI_FATAL(ns, code) \
	{ \
		napi_status _status = code; \
		if (_status == napi_pending_exception) { \
			napi_value fatal; \
			::napi_get_and_clear_last_exception(env, &fatal); \
			::napi_fatal_exception(env, fatal); \
			return; \
		} else if (_status != napi_ok) { \
			const napi_extended_error_info* error; \
			::napi_get_last_error_info(env, &error); \
			\
			char msg[1024]; \
			::snprintf(msg, 1024, ns ": " #code " failed (status=%d) %s", _status, error->error_message); \
			\
			napi_value fatal; \
			if (::napi_create_string_utf8(env, msg, strlen(msg), &fatal) == napi_ok) { \
				::napi_fatal_exception(env, fatal); \
			} else { \
				::fprintf(stderr, "%s", msg); \
			} \
			return; \
		} \
	}

#ifdef ENABLE_RAW_DEBUGGING
	#define LOG_DEBUG_VARS
	#define LOG_DEBUG_EXTERN_VARS
	#define LOG_DEBUG(ns, msg) \
		{ \
			std::string str(msg); \
			::fprintf(stderr, "%s: %s\n", ns, str.c_str()); \
		}
#else
	#define LOG_DEBUG_VARS \
		std::mutex logLock; \
		uv_async_t logNotify; \
		std::queue<std::shared_ptr<node_ios_device::LogMessage>> logQueue;

	#define LOG_DEBUG_EXTERN_VARS \
		extern uv_thread_t mainThread; \
		extern std::mutex logLock; \
		extern uv_async_t logNotify; \
		extern std::queue<std::shared_ptr<node_ios_device::LogMessage>> logQueue;

	#define LOG_DEBUG(ns, msg) \
		{ \
			std::string str(msg); \
			std::shared_ptr<node_ios_device::LogMessage> obj = std::make_shared<node_ios_device::LogMessage>(ns, msg); \
			std::lock_guard<std::mutex> lock(node_ios_device::logLock); \
			node_ios_device::logQueue.push(obj); \
			::uv_async_send(&node_ios_device::logNotify); \
		}
#endif

#define LOG_DEBUG_FORMAT(ns, code) \
	{ \
		char buffer[1024]; \
		::code; \
		LOG_DEBUG(ns, buffer) \
	}

#define LOG_DEBUG_1(ns, msg, a1)             LOG_DEBUG_FORMAT(ns, snprintf(buffer, 1024, msg, a1))
#define LOG_DEBUG_2(ns, msg, a1, a2)         LOG_DEBUG_FORMAT(ns, snprintf(buffer, 1024, msg, a1, a2))
#define LOG_DEBUG_3(ns, msg, a1, a2, a3)     LOG_DEBUG_FORMAT(ns, snprintf(buffer, 1024, msg, a1, a2, a3))
#define LOG_DEBUG_4(ns, msg, a1, a2, a3, a4) LOG_DEBUG_FORMAT(ns, snprintf(buffer, 1024, msg, a1, a2, a3, a4))

#define LOG_DEBUG_THREAD_ID(ns, msg) \
	LOG_DEBUG_1(ns, msg " (thread %zu)", std::hash<std::thread::id>{}(std::this_thread::get_id()))

#define LOG_DEBUG_THREAD_ID_2(ns, msg, a1, a2) \
	LOG_DEBUG_3(ns, msg " (thread %zu)", a1, a2, std::hash<std::thread::id>{}(std::this_thread::get_id()))

#endif
