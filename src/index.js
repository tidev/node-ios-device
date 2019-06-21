const binding = require('node-gyp-build')(`${__dirname}/..`);
const { EventEmitter } = require('events');
const fs = require('fs');
const logger = require('snooplogg').default('node-ios-device');
const nss = {};
const path = require('path');

/**
 * The `node-ios-device` API and debug log emitter.
 *
 * @type {EventEmitter}
 * @emits {log} Emits a debug log message.
 */
const api = module.exports = new EventEmitter();

// init node-ios-device's debug logging and device manager.
binding.init((ns, msg) => {
	api.emit('log', msg);
	if (ns) {
		(nss[ns] || (nss[ns] = logger(ns))).log(msg);
	} else {
		logger.log(msg);
	}
});

/**
 * Relays syslog messages.
 *
 * @param {String} udid - The device udid to install the app to.
 * @param {Number} port - The port number to connect to and forward messages from.
 * @returns {Promise<EventEmitter>} Resolves a handle to wire up listeners and stop watching.
 * @emits {data} Emits a buffer containing syslog messages.
 * @emits {end} Emits when the device has been disconnected.
 */
api.forward = function forward(udid, port) {
	if (!udid || typeof udid !== 'string') {
		throw new TypeError('Expected udid to be a non-empty string');
	}

	const handle = new EventEmitter();
	const emit = handle.emit.bind(handle);
	port = ~~port;

	handle.stop = function stop() {
		binding.stopForward(udid, port, emit);
	};
	binding.startForward(udid, port, emit);

	return handle;
};

/**
 * Installs an iOS app on the specified device.
 *
 * @param {String} udid - The device udid to install the app to.
 * @param {String} appPath - The path to iOS .app directory to install.
 */
api.install = function install(udid, appPath) {
	if (!udid || typeof udid !== 'string') {
		throw new TypeError('Expected udid to be a non-empty string');
	}

	if (!appPath || typeof appPath !== 'string') {
		throw new TypeError('Expected app path to be a non-empty string');
	}

	appPath = path.resolve(appPath);

	try {
		fs.statSync(appPath);
	} catch (e) {
		throw new Error(`App not found: ${appPath}`);
	}

	try {
		if (!fs.statSync(path.join(appPath, 'PkgInfo')).isFile()) {
			throw new Error();
		}
	} catch (e) {
		throw new Error(`Invalid app: ${appPath}`);
	}

	binding.install(udid, appPath);
};

/**
 * Returns a list of all connected iOS devices.
 *
 * @returns {Array.<Object>}
 */
api.list = binding.list;

/**
 * Relays syslog messages.
 *
 * @param {String} udid - The device udid to install the app to.
 * @returns {Promise<EventEmitter>} Resolves a handle to wire up listeners and stop watching.
 * @emits {data} Emits a buffer containing syslog messages.
 * @emits {end} Emits when the device has been disconnected.
 */
api.syslog = function syslog(udid) {
	if (!udid || typeof udid !== 'string') {
		throw new TypeError('Expected udid to be a non-empty string');
	}

	const handle = new EventEmitter();
	const emit = handle.emit.bind(handle);

	handle.stop = function stop() {
		binding.stopSyslog(udid, emit);
	};
	binding.startSyslog(udid, emit);

	return handle;
};

/**
 * Watches a key for changes to subkeys and values.
 *
 * @returns {EventEmitter} The handle to wire up listeners and stop watching.
 * @emits {change} Emits an array of device objects.
 */
api.watch = function watch() {
	const handle = new EventEmitter();
	const emit = handle.emit.bind(handle);

	handle.stop = function stop() {
		binding.unwatch(emit);
	};
	binding.watch(emit);

	return handle;
};
