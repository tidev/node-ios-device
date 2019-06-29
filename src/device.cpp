#include "device.h"
#include <sstream>

namespace node_ios_device {

/**
 * Initializes the device by creating the relays, initializing the supplied device interface, and
 * retrieving the device properties.
 *
 * Not that we only need to get the props from the first device interface since it's the same
 * regardless of the which interface.
 */
Device::Device(napi_env env, std::string& udid, am_device& dev, std::weak_ptr<CFRunLoopRef> runloop) :
	portRelay(env, runloop),
	syslogRelay(env, runloop),
	env(env),
	udid(udid) {

	auto iface = config(dev, true);

	LOG_DEBUG_1("Device", "Getting device info for %s", udid.c_str());
	iface->connect();

	props["name"]            = std::make_unique<DeviceProp>(iface->getString(CFSTR("DeviceName")));
	props["buildVersion"]    = std::make_unique<DeviceProp>(iface->getString(CFSTR("BuildVersion")));
	props["cpuArchitecture"] = std::make_unique<DeviceProp>(iface->getString(CFSTR("CPUArchitecture")));
	props["deviceClass"]     = std::make_unique<DeviceProp>(iface->getString(CFSTR("DeviceClass")));

	std::string deviceColor = iface->getString(CFSTR("DeviceColor"));
	if (deviceColor == "0") {
		deviceColor = "White";
	} else if (deviceColor == "1") {
		deviceColor = "Black";
	} else if (deviceColor == "2") {
		deviceColor = "Silver";
	} else if (deviceColor == "3") {
		deviceColor = "Gold";
	} else if (deviceColor == "4") {
		deviceColor = "Rose Gold";
	} else if (deviceColor == "5") {
		deviceColor = "Jet Black";
	}
	props["deviceColor"] = std::make_unique<DeviceProp>(deviceColor);

	props["hardwareModel"]       = std::make_unique<DeviceProp>(iface->getString(CFSTR("HardwareModel")));
	props["modelNumber"]         = std::make_unique<DeviceProp>(iface->getString(CFSTR("ModelNumber")));
	props["productType"]         = std::make_unique<DeviceProp>(iface->getString(CFSTR("ProductType")));
	props["productVersion"]      = std::make_unique<DeviceProp>(iface->getString(CFSTR("ProductVersion")));
	props["serialNumber"]        = std::make_unique<DeviceProp>(iface->getString(CFSTR("SerialNumber")));
	props["trustedHostAttached"] = std::make_unique<DeviceProp>(iface->getBoolean(CFSTR("TrustedHostAttached")));

	iface->disconnect();
}

/**
 * Adds or removes a device interface.
 */
DeviceInterface* Device::config(am_device& dev, bool isAdd) {
	uint32_t type = ::AMDeviceGetInterfaceType(dev);
	if (type == 1) {
		if (isAdd && !usb) {
			LOG_DEBUG_1("Device::config", "Device %s connected via USB", udid.c_str())
			usb = std::make_shared<DeviceInterface>(udid, dev);
			return usb.get();
		} else if (!isAdd && usb) {
			LOG_DEBUG_1("Device::config", "Device %s disconnected via USB", udid.c_str())
			usb = nullptr;
		}
	} else if (type == 2) {
		if (isAdd && !wifi) {
			LOG_DEBUG_1("Device::config", "Device %s connected via Wi-Fi", udid.c_str())
			wifi = std::make_shared<DeviceInterface>(udid, dev);
			return wifi.get();
		} else if (!isAdd && wifi) {
			LOG_DEBUG_1("Device::config", "Device %s disconnected via Wi-Fi", udid.c_str())
			wifi = nullptr;
		}
	} else {
		throw std::runtime_error("Unknown device interface type");
	}

	return NULL;
}

/**
 * Starts or stops port forwarding.
 */
void Device::forward(uint8_t action, napi_value nport, napi_value listener) {
	if (action == RELAY_START && !usb) {
		throw std::runtime_error("Port forward requires a USB connected iOS device");
	}
	portRelay.config(action, nport, listener, usb);
}

/**
 * Installs the specified app on the device.
 */
void Device::install(std::string& appPath) {
	if (usb) {
		usb->install(appPath);
	} else if (wifi) {
		wifi->install(appPath);
	} else {
		std::stringstream error;
		error << "No interfaces found for device " << udid;
		throw std::runtime_error(error.str());
	}
}

/**
 * Starts or stops syslog relaying.
 */
void Device::syslog(uint8_t action, napi_value listener) {
	if (action == RELAY_START && !usb) {
		throw std::runtime_error("syslog requires a USB connected iOS device");
	}
	syslogRelay.config(action, listener, usb);
}

/**
 * Serialized the device info to a JavaScript object.
 */
napi_value Device::toJS() {
	napi_value obj;

	NAPI_THROW_RETURN("Device::toJS", "ERR_NAPI_CREATE_OBJECT", ::napi_create_object(env, &obj), NULL)

	{
		napi_value tmp;
		NAPI_THROW_RETURN("Device::toJS", "ERR_NAPI_CREATE_STRING", ::napi_create_string_utf8(env, udid.c_str(), NAPI_AUTO_LENGTH, &tmp), NULL)
		NAPI_THROW_RETURN("Device::toJS", "ERR_NAPI_SET_NAMED_PROPERTY", ::napi_set_named_property(env, obj, "udid", tmp), NULL)
	}

	{
		napi_value ifaces, push, type;
		NAPI_THROW_RETURN("Device::toJS", "ERR_NAPI_CREATE_ARRAY", ::napi_create_array(env, &ifaces), NULL)
		NAPI_THROW_RETURN("Device::toJS", "ERR_NAPI_GET_NAMED_PROPERTY", ::napi_get_named_property(env, ifaces, "push", &push), NULL)

		if (usb) {
			NAPI_THROW_RETURN("Device::toJS", "ERR_NAPI_CREATE_STRING", ::napi_create_string_utf8(env, "USB", NAPI_AUTO_LENGTH, &type), NULL)
			NAPI_THROW_RETURN("Device::toJS", "ERR_NAPI_CALL_FUNCTION", ::napi_call_function(env, ifaces, push, 1, &type, NULL), NULL)
		}

		if (wifi) {
			NAPI_THROW_RETURN("Device::toJS", "ERR_NAPI_CREATE_STRING", ::napi_create_string_utf8(env, "Wi-Fi", NAPI_AUTO_LENGTH, &type), NULL)
			NAPI_THROW_RETURN("Device::toJS", "ERR_NAPI_CALL_FUNCTION", ::napi_call_function(env, ifaces, push, 1, &type, NULL), NULL)
		}

		NAPI_THROW_RETURN("Device::toJS", "ERR_NAPI_SET_NAMED_PROPERTY", ::napi_set_named_property(env, obj, "interfaces", ifaces), NULL)
	}

	for (auto const& it : props) {
		napi_value tmp;
		if (it.second->type == Boolean) {
			NAPI_THROW_RETURN("Device::toJS", "ERR_NAPI_GET_BOOLEAN", ::napi_get_boolean(env, std::get<bool>(it.second->value), &tmp), NULL)
		} else if (it.second->type == String) {
			NAPI_THROW_RETURN("Device::toJS", "ERR_NAPI_CREATE_STRING", ::napi_create_string_utf8(env, std::get<std::string>(it.second->value).c_str(), NAPI_AUTO_LENGTH, &tmp), NULL)
		} else {
			continue;
		}
		NAPI_THROW_RETURN("Device::toJS", "ERR_NAPI_SET_NAMED_PROPERTY", ::napi_set_named_property(env, obj, it.first, tmp), NULL)
	}

	return obj;
}

}
