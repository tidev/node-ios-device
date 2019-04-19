/**
 * Public API for the node-ios-device library.
 *
 * @module ios-device
 *
 * @copyright
 * Copyright (c) 2012-2019 by Appcelerator, Inc. All Rights Reserved.
 *
 * @license
 * Licensed under the terms of the Apache Public License
 * Please see the LICENSE included with this distribution for details.
 */

'use strict';

const fs = require('fs');
const init = require('node-pre-gyp-init');
const path = require('path');

const snooplogg = require('snooplogg').default;
const logger = snooplogg('node-ios-device');
const { highlight } = snooplogg.styles;
const { EventEmitter } = require('events');

let binding;
let activeCalls = 0;
const emitter = new EventEmitter();

const deviceColors = {
	'0': 'White',
	'1': 'Black',
	'2': 'Silver',
	'3': 'Gold',
	'4': 'Rose Gold',
	'5': 'Jet Black'
};

emitter.on('debug', logger.log);

exports.devices = devices;
exports.trackDevices = trackDevices;
exports.installApp = installApp;
exports.log = log;

/**
 * Exposes both an event emitter API and a `stop()` method for canceling long
 * running functions such as `trackDevices()` and `log()`.
 */
class Handle extends EventEmitter {
	stop() {
		// meant to be overwritten
	}
}

/**
 * Retrieves an array of all connected iOS devices.
 *
 * @returns {Promise} Resolves the list of connected devices.
 */
async function devices(callback) {
	await initBinding();

	logger.log('Calling binding.devices()');
	activeCalls++;

	return await new Promise((resolve, reject) => {
		binding.devices((err, devices) => {
			if (--activeCalls === 0) {
				binding.suspend();
			}
			if (err) {
				reject(err);
			} else {
				resolve(processDevices(devices));
			}
		});
	});
}

/**
 * Internal helper function that initializes the node-ios-device binding.
 *
 * @returns {Promise}
 */
async function initBinding() {
	if (process.platform !== 'darwin') {
		throw new Error(`${process.platform} not supported`);
	}

	if (binding) {
		return;
	}

	logger.log('Initializing binding');

	await new Promise((resolve, reject) => {
		init(path.resolve(__dirname, './package.json'), function (err, bindingPath) {
			if (err) {
				return reject(err);
			}

			logger.log(`Loading binding: ${highlight(bindingPath)}`);
			binding = require(bindingPath);

			logger.log('Initializing node-ios-device and setting emitter');
			binding.initialize(emitter);

			resolve();
		});
	});
}

/**
 * Installs an iOS app on the specified device.
 *
 * @param {String} udid - The device udid to install the app to.
 * @param {String} appPath - The path to iOS .app directory to install.
 * @returns {Promise}
 */
async function installApp(udid, appPath) {
	await initBinding();

	appPath = path.resolve(appPath);

	try {
		if (!fs.statSync(path.join(appPath, 'PkgInfo')).isFile()) {
			throw new Error('Specified .app path is not a valid app');
		}
	} catch (e) {
		return callback(new Error('Specified .app path does not exist'));
	}

	activeCalls++;
	binding.resume();

	await new Promise(resolve => {
		binding.installApp(udid, appPath, () => {
			resolve();
			if (--activeCalls === 0) {
				binding.suspend();
			}
		});
	});
}

/**
 * Forwards the specified iOS device's log messages.
 *
 * @param {String} udid - The device udid to forward log messages.
 * @param {Number} [port] - An optional port number to connect to on the device,
 * otherwise assumes syslog.
 * @returns {Handle} A handle that emits a `log` event and `stop()` method.
 */
function log(udid, port) {
	if (typeof port !== 'undefined' && (typeof port !== 'number' || port < 1 || port > 65535)) {
		throw new Error('Port must be a number between 1 and 65535');
	}

	const handle = new Handle();
	let running = false;
	const evtName = port ? `LOG_PORT_${port}_${udid}` : `SYSLOG_${udid}`;
	const emitFn = msg => handle.emit('log', msg);
	emitter.on(evtName, emitFn);
	let timer = null;

	const tryStartLogRelay = () => {
		try {
			binding.startLogRelay(udid, port);
			running = true;
			handle.emit('app-started');
		} catch (e) {
			timer = setTimeout(tryStartLogRelay, 250);
		}
	};

	emitter.on('app-quit', _port => {
		if (~~_port === port) {
			handle.emit('app-quit');
			setImmediate(tryStartLogRelay);
		}
	});

	const trackHandle = trackDevices()
		.on('devices', devices => {
			clearTimeout(timer);
			logger.log(`Connected devices: ${devices.map(dev => dev.udid).join(', ')}`);
			if (devices.some(dev => dev.udid === udid)) {
				tryStartLogRelay();
			} else if (running) {
				// device was disconnected
				logger.log('Device was disconnected');
				handle.emit('disconnect');
			} else {
				// device was never connected
				handle.stop();
				handle.emit('error', new Error(`Device '${udid}' not connected`));
			}
		})
		.on('error', err => {
			handle.stop();
			handle.emit('error', err);
		});

	handle.stop = () => {
		clearTimeout(timer);
		running = false;
		emitter.removeListener(evtName, emitFn);

		if (!emitter._events[evtName] || emitter._events[evtName].length <= 0) {
			try {
				// if the device has been disconnected before this stop()
				// function has been called, then node-ios-device will throw an
				// error that it can't stop relaying the log since the device
				// has been disconnected
				binding.stopLogRelay(udid, port);
			} catch (e) {
				// squelch
			}
		}

		trackHandle.stop();
	};

	return handle;
}

/**
 * Fixes the device color.
 *
 * @param {Array.<Object>} devices - A list of devices.
 * @returns {Array.<Object>} The original list of devices.
 */
function processDevices(devices) {
	for (const device of devices) {
		const color = deviceColors[device.deviceColor];
		if (color) {
			device.deviceColor = color;
		} else {
			device.deviceColor = device.deviceColor.substring(0, 1).toUpperCase() + device.deviceColor.substring(1);
		}
	}
	return devices;
}

/**
 * Continuously retrieves an array of all connected iOS devices. Whenever a
 * device is connected or disconnected, the specified callback is fired.
 *
 * @returns {Handle} A handle that emits a `log` event and `stop()` method.
 */
function trackDevices() {
	const handle = new Handle();
	let running = false;
	const onDevicesChanged = suppressLog => {
		if (running) {
			if (!suppressLog) {
				logger.log('Devices changed, calling callback');
			}
			binding.devices((err, devices) => {
				if (err) {
					handle.stop();
					handle.emit('error', err);
				} else {
					handle.emit('devices', processDevices(devices));
				}
			});
		}
	};

	handle.stop = () => {
		if (running) {
			running = false;
			emitter.removeListener('devicesChanged', onDevicesChanged);
			if (--activeCalls === 0) {
				binding.suspend();
			}
		}
	};

	setImmediate(async () => {
		try {
			await initBinding();
		} catch (err) {
			return handle.emit('error', err);
		}

		activeCalls++;
		running = true;

		onDevicesChanged(true);

		emitter.on('devicesChanged', onDevicesChanged);
		binding.resume();
	});

	return handle;
}
