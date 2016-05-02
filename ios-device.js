/**
 * Public API for the node-ios-device library.
 *
 * @module ios-device
 *
 * @copyright
 * Copyright (c) 2012-2016 by Appcelerator, Inc. All Rights Reserved.
 *
 * @license
 * Licensed under the terms of the Apache Public License
 * Please see the LICENSE included with this distribution for details.
 */

'use strict';

var binary = require('node-pre-gyp');
var exec = require('child_process').exec;
var fs = require('fs');
var path = require('path');
var binding = require(binary.find(path.resolve(__dirname, './package.json')));
var platformErrorMsg = 'OS "' + process.platform + '" not supported';

// reference counter to track how many trackDevice() calls are active
var pumping = 0;
var timer;

module.exports.pumpInterval = 10;
module.exports.devices = devices;
module.exports.trackDevices = trackDevices;
module.exports.installApp = installApp;
module.exports.log = log;

/**
 * Retrieves an array of all connected iOS devices.
 *
 * @param {Function} callback(err, devices) - A function to call with the connected devices.
 */
function devices(callback) {
	if (process.platform !== 'darwin') {
		return callback(new Error(platformErrorMsg));
	}

	binding.pumpRunLoop();
	callback(null, binding.devices());
}

/**
 * Continuously retrieves an array of all connected iOS devices. Whenever a
 * device is connected or disconnected, the specified callback is fired.
 *
 * @param {Function} callback(err, devices) - A function to call with the connected devices.
 * @returns {Function} off() - A function that discontinues tracking.
 */
function trackDevices(callback) {
	if (process.platform !== 'darwin') {
		return callback(new Error(platformErrorMsg));
	}

	// if we're not already pumping, start up the pumper
	if (!pumping) {
		pump();
	}
	pumping++;

	// immediately return the array of devices
	exports.devices(callback);

	var stopped = false;

	// listen for any device connects or disconnects
	binding.on('devicesChanged', function (devices) {
		stopped || callback(null, binding.devices());
	});

	// return the stop() function
	return function () {
		if (!stopped) {
			stopped = true;
			pumping = Math.max(pumping - 1, 0);
			pumping || clearTimeout(timer);
		}
	};
}

/**
 * Installs an iOS app on the specified device.
 *
 * @param {String} udid - The device udid to install the app to.
 * @param {String} appPath - The path to iOS .app directory to install.
 * @param {Function} callback(err) - A function to call when the install finishes.
 */
function installApp(udid, appPath, callback) {
	if (process.platform !== 'darwin') {
		return callback(new Error(platformErrorMsg));
	}

	appPath = path.resolve(appPath);

	if (!fs.existsSync(appPath)) {
		return callback(new Error('Specified .app path does not exist'));
	}
	if (!fs.statSync(appPath).isDirectory() || !fs.existsSync(path.join(appPath, 'PkgInfo'))) {
		return callback(new Error('Specified .app path is not a valid app'));
	}

	binding.pumpRunLoop();

	try {
		binding.installApp(udid, appPath);
		callback(null);
	} catch (ex) {
		callback(ex);
	}
}

/**
 * Forwards the specified iOS device's log messages.
 *
 * @param {String} udid - The device udid to forward log messages.
 * @param {Function} callback(err) - A function to call with each log message.
 */
function log(udid, callback) {
	if (process.platform !== 'darwin') {
		return callback(new Error(platformErrorMsg));
	}

	// if we're not already pumping, start up the pumper
	if (!pumping) {
		pump();
	}
	pumping++;

	var stopped = false;

	binding.log(udid, function (msg) {
		stopped || callback(msg);
	});

	// return the off() function
	return function () {
		if (!stopped) {
			stopped = true;
			pumping = Math.max(pumping - 1, 0);
			pumping || clearTimeout(timer);
		}
	};
}

/**
 * Ticks the CoreFoundation run loop.
 */
function pump() {
	binding.pumpRunLoop();
	timer = setTimeout(pump, exports.pumpInterval);
}
