#include "device-interface.h"
#include <sstream>

namespace node_ios_device {

DeviceInterface::DeviceInterface(std::string& udid, am_device& dev) :
	dev(dev), udid(udid), numConnections(0) {}

DeviceInterface::~DeviceInterface() {
	disconnect(true);
}

/**
 * Connects to the device, pairs with it, and starts a session. We use a
 * connected counter so that we don't connect more than once.
 */
void DeviceInterface::connect() {
		if (numConnections > 0) {
		LOG_DEBUG("Device::connect", "Already connected")
		return;
	}

	if (numConnections < 0) {
		numConnections = 0;
	}
	++numConnections;

	try {
		// connect to the device
		LOG_DEBUG_1("Device::connect", "Connecting to device: %s", udid.c_str())
		mach_error_t rval = ::AMDeviceConnect(dev);
		if (rval == MDERR_SYSCALL) {
			throw std::runtime_error("Failed to connect to device: setsockopt() failed");
		} else if (rval == MDERR_QUERY_FAILED) {
			throw std::runtime_error("Failed to connect to device: the daemon query failed");
		} else if (rval == MDERR_INVALID_ARGUMENT) {
			throw std::runtime_error("Failed to connect to device: invalid argument, USBMuxConnectByPort returned 0xffffffff");
		} else if (rval != MDERR_OK) {
			std::stringstream error;
			error << "Failed to connect to device (0x" << std::hex << rval << ")";
			throw std::runtime_error(error.str());
		}

		// if we're not paired, go ahead and pair now
		LOG_DEBUG_1("Device::connect", "Pairing device: %s", udid.c_str())
		if (::AMDeviceIsPaired(dev) != 1 && ::AMDevicePair(dev) != 1) {
			throw std::runtime_error("Failed to pair device");
		}

		// double check the pairing
		LOG_DEBUG("Device::connect", "Validating device pairing");
		rval = ::AMDeviceValidatePairing(dev);
		if (rval == MDERR_INVALID_ARGUMENT) {
			throw std::runtime_error("Device is not paired: the device is null");
		} else if (rval == MDERR_DICT_NOT_LOADED) {
			throw std::runtime_error("Device is not paired: load_dict() failed");
		} else if (rval != MDERR_OK) {
			std::stringstream error;
			error << "Device is not paired (0x" << std::hex << rval << ")";
			throw std::runtime_error(error.str());
		}

		// start the session
		LOG_DEBUG_1("Device::connect", "Starting session: %s", udid.c_str())
		rval = ::AMDeviceStartSession(dev);
		if (rval == MDERR_INVALID_ARGUMENT) {
			throw std::runtime_error("Failed to start session: the lockdown connection has not been established");
		} else if (rval == MDERR_DICT_NOT_LOADED) {
			throw std::runtime_error("Failed to start session: load_dict() failed");
		} else if (rval != MDERR_OK) {
			std::stringstream error;
			error << "Failed to start session (0x" << std::hex << rval << ")";
			throw std::runtime_error(error.str());
		}
	} catch (std::runtime_error& e) {
		disconnect(dev);
		throw e;
	}

}

/**
 * Disconnects the device if there are no other active connections to this
 * device. Generally, force should not be set. It's mainly there for the
 * destructor.
 */
void DeviceInterface::disconnect(const bool force) {
	if (dev && numConnections > 0) {
		if (force || numConnections == 1) {
			LOG_DEBUG_1("Device::disconnect", "Stopping session: %s", udid.c_str())
			::AMDeviceStopSession(dev);
			LOG_DEBUG_1("Device::disconnect", "Disconnecting from device: %s", udid.c_str())
			::AMDeviceDisconnect(dev);
			numConnections = 0;
		} else {
			--numConnections;
		}
	}
}

std::string DeviceInterface::getProp(CFStringRef key) {
	CFStringRef value = (CFStringRef)::AMDeviceCopyValue(dev, 0, key);
	return value ? std::string(::CFStringGetCStringPtr(value, kCFStringEncodingUTF8)) : "";
}

void DeviceInterface::install(std::string& appPath) {
	CFStringRef appPathStr = ::CFStringCreateWithCString(NULL, appPath.c_str(), kCFStringEncodingUTF8);
	CFURLRef relativeUrl = ::CFURLCreateWithFileSystemPath(NULL, appPathStr, kCFURLPOSIXPathStyle, false);
	CFURLRef localUrl = ::CFURLCopyAbsoluteURL(relativeUrl);
	::CFRelease(appPathStr);
	::CFRelease(relativeUrl);

	CFStringRef keys[] = { CFSTR("PackageType") };
	CFStringRef values[] = { CFSTR("Developer") };
	CFDictionaryRef options = CFDictionaryCreate(NULL, (const void **)&keys, (const void **)&values, 1, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);

	connect();

	LOG_DEBUG_1("Device::install", "Transferring app to device: %s", udid.c_str())
	mach_error_t rval = ::AMDeviceSecureTransferPath(0, dev, localUrl, options, NULL, 0);
	if (rval != MDERR_OK) {
		::CFRelease(options);
		::CFRelease(localUrl);
		disconnect();
		if (rval == -402653177) {
			throw std::runtime_error("Failed to copy app to device: can't install app that contains symlinks");
		}
		std::stringstream error;
		error << "Failed to transfer app to device (0x" << std::hex << rval << ")";
		throw std::runtime_error(error.str());
	}

	// install package on device
	LOG_DEBUG_1("Device::install", "Installing app on device: %s", udid.c_str());
	rval = ::AMDeviceSecureInstallApplication(0, dev, localUrl, options, NULL, 0);
	::CFRelease(options);
	::CFRelease(localUrl);
	disconnect();

	if (rval == -402620395) {
		throw std::runtime_error("Failed to install app on device: most likely a provisioning profile issue");
	} else if (rval != MDERR_OK) {
		std::stringstream error;
		error << "Failed to install app on device (0x" << std::hex << rval << ")";
		throw std::runtime_error(error.str());
	}
}

/**
 * Starts a service.
 * Note that if the call to AMDeviceStartService() fails, it's probably because MobileDevice thinks
 * we're connected and paired, but we're not.
 */
void DeviceInterface::startService(const char* serviceName, service_conn_t* connection) {
	connect();

	LOG_DEBUG_2("DeviceInterface::startService", "Starting \'%s\' service: %s", serviceName, udid.c_str());
	mach_error_t rval = ::AMDeviceStartService(dev, ::CFStringCreateWithCStringNoCopy(NULL, serviceName, kCFStringEncodingUTF8, NULL), connection, NULL);

	disconnect();

	std::stringstream error;
	if (rval == MDERR_SYSCALL) {
		error << "Failed to start \"" << serviceName << "\" service due to system call error (0x" << std::hex << rval << ")";
		throw std::runtime_error(error.str());
	} else if (rval == MDERR_INVALID_ARGUMENT) {
		error << "Failed to start \"" << serviceName << "\" service due to invalid argument (0x" << std::hex << rval << ")";
		throw std::runtime_error(error.str());
	} else if (rval != MDERR_OK) {
		error << "Failed to start \"" << serviceName << "\" service (0x" << std::hex << rval << ")";
		throw std::runtime_error(error.str());
	}
}

}
