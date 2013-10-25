# node-ios-device

Queries connected iOS devices and installs apps.

[![NPM](https://nodei.co/npm/node-ios-device.png?downloads=true&stars=true)](https://nodei.co/npm/node-ios-device/)

## Prerequisites

node-ios-device requires [Node.js](http://nodejs.org/dist/) v0.8 or newer including v0.11 (unstable).

Currently, only Mac OS X is supported.

## Installation

From NPM:

	npm install node-ios-device

From GitHub:

	npm install git://github.com/appcelerator/node-ios-device.git

From Source:

	git clone https://github.com/appcelerator/node-ios-device.git
	cd node-ios-device
	make

## Example

	var iosDevice = require('node-ios-device');

	// get all connected iOS devices
	iosDevice.devices(function (err, devices) {
		console.log('Connected devices:');
		console.log(devices);
	});

	// continuously watch for devices to be conected or disconnected
	iosDevice.trackDevices(function (err, devices) {
		console.log('Connected devices:');
		console.log(devices);
	});

	// install an iOS app
	iosDevice.installApp('<device udid>', '/path/to/my.app', function (err) {
		if (err) {
			console.error(err);
		} else {
			console.log('Success!');
		}
	});

## API

### devices(callback)

Retrieves an array of all connected iOS devices.

* `{function} callback(err, devices)` - A function to call with the connected devices
	* `{null|Error} err` - An `Error` if there was a problem, otherwise `null`
	* `{Array<Object>} devices` - An array of Device objects

Device objects contain the following information:

* `udid` - The device's unique device id (e.g. "a4cbe14c0441a2bf87f397602653a4ac71eb0336")
* `name` - The name of the device (e.g. "My iPhone")
* `buildVersion` - The build version (e.g. "10B350")
* `cpuArchitecture` - The CPU architecture (e.g. "armv7s")
* `deviceClass` - The type of device (e.g. "iPhone", "iPad")
* `deviceColor` - The color of the device (e.g. "black")
* `hardwareModel` - The device module (e.g. "[N41AP](http://theiphonewiki.com/wiki/N41ap)")
* `modelNumber` - The model number (e.g. "MD636")
* `productType` - The product type or model id (e.g. "iPhone5,1")
* `productVersion` - The iOS version (e.g. "6.1.4")
* `serialNumber` - The device serial number (e.g. "XXXXXXXXXXXX")

There is more data that could have been retrieved from the device, but the
properties above seemed the most reasonable.

### trackDevices(callback)

Continuously retrieves an array of all connected iOS devices. Whenever a device
is connected or disconnected, the specified callback is fired.

* `{Function} callback(err, devices)` - A function to call with the connected devices
	* `{null|Error} err` - An `Error` if there was a problem, otherwise `null`
	* `{Array<Object>} devices` - An array of Device objects

Returns a function to discontinue tracking:

	var off = iosDevice.trackDevices(function (err, devices) {
		console.log('Connected devices:');
		console.log(devices);
	});

	setTimeout(function () {
		// turn off tracking after 1 minute
		off();
	}, 60000);

### installApp(udid, appPath, callback)

Installs an iOS app on the specified device.

* `{String} udid` - The devices udid
* `{String} appPath` - The path to the iOS .app
* `{Function} callback(err)` - A function to call when the install finishes
	* `{null|Error} err` - An `Error` if there was a problem, otherwise `null`

Currently, an `appPath` that begins with `~` is not supported.

The `appPath` must resolve to an iOS .app, not the .ipa file.

## License

This project is open source and provided under the Apache Public License
(version 2). Please make sure you see the `LICENSE` file included in this
distribution for more details on the license.  Also, please take notice of the
privacy notice at the end of the file.

This project contains `mobiledevice.h` from https://bitbucket.org/tristero/mobiledeviceaccess
and is available under public domain.

#### (C) Copyright 2012-2013, [Appcelerator](http://www.appcelerator.com/) Inc. All Rights Reserved.
