#ifndef NODE_IOS_DEVICE_MACRO_H
#define NODE_IOS_DEVICE_MACRO_H

#define NAPI_INIT() \
	static void napi_macros_init(napi_env env, napi_value exports); \
	static napi_value napi_macros_init_wrap(napi_env env, napi_value exports) { \
		napi_macros_init(env, exports); \
		return exports; \
	} \
	NAPI_MODULE(NODE_GYP_MODULE_NAME, napi_macros_init_wrap) \
	static void napi_macros_init(napi_env env, napi_value exports)

#define NAPI_STATUS_THROWS_VOID(call) \
	if ((call) != napi_ok) { \
		napi_throw_error(env, NULL, #call " failed!"); \
		return; \
	}

#define NAPI_STATUS_THROWS(call) \
	if ((call) != napi_ok) { \
		napi_throw_error(env, NULL, #call " failed!"); \
		return NULL; \
	}

#define NAPI_METHOD(name) \
	napi_value name(napi_env env, napi_callback_info info)

#define NAPI_EXPORT_FUNCTION(name) \
	{ \
		napi_value name##_fn; \
		NAPI_STATUS_THROWS_VOID(napi_create_function(env, NULL, 0, name, NULL, &name##_fn)) \
		NAPI_STATUS_THROWS_VOID(napi_set_named_property(env, exports, #name, name##_fn)) \
	}

#define NAPI_ARGV(n) \
	napi_value argv[n]; \
	size_t argc = n; \
	NAPI_STATUS_THROWS(napi_get_cb_info(env, info, &argc, argv, NULL, NULL))

#endif
