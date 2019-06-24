# node-ios-device [![Build Status][3]][4] [![Greenkeeper badge][5]][6]

Queries connected iOS devices, installs apps, and relays log output.

## Prerequisites

`node-ios-device` only works on macOS and is currently compatible with the following versions:

 * Node.js
   * v8.12.x or newer
   * v10.2.0 or newer

## Installation

    npm install node-ios-device

## CLI

```
$ node-ios-device

USAGE: node-ios-device <command> [options]

COMMANDS:
  forward  Connects to a port on an device and relays messages
  install  Install an app on the specified device
  list     Lists connected devices
  syslog   Outputs a devices syslog messages
  watch    Listens for devices to be connected/disconnected

GLOBAL OPTIONS:
  --no-color    Disable colors
  -h,--help     Displays the help screen
  -v,--version  Outputs the version
```

## Example

```js
import iosDevice from 'node-ios-device';

// get all connected iOS devices
const devices = iosDevice.list();
console.log('Connected devices:', devices);

// continuously watch for devices to be connected or disconnected
const handle = iosDevice.watch();
handle.on('change', devices => {
    console.log('Connected devices:', devices);
});
handle.on('error', console.error);

// install an iOS app
iosDevice.install('<device udid>', '/path/to/my.app');
console.log('Success!');

// relay the syslog output to the console
iosDevice
    .syslog('<device udid>')
    .on('data', console.log)
    .on('end', () => console.log('Device disconnected'));

// relay output from a TCP port created by an iOS app
iosDevice
    .forward('<device udid>', 1337)
    .on('data', console.log)
    .on('end', () => console.log('Device disconnected'));
```

## API

### `list()`

Retrieves an array of all connected iOS devices.

Returns an `Array` of device objects.

Note that only devices connected via a USB cable will be returned. Devices connected via Wi-Fi will
not be returned. The main reason we do this is because you can only relay the syslog from USB
connected devices.

Device objects contain the following information:

* `udid` - The device's unique device id (e.g. "a4cbe14c0441a2bf87f397602653a4ac71eb0336")
* `name` - The name of the device (e.g. "My iPhone")
* `buildVersion` - The build version (e.g. "10B350")
* `cpuArchitecture` - The CPU architecture (e.g. "armv7s")
* `deviceClass` - The type of device (e.g. "iPhone", "iPad")
* `deviceColor` - The color of the device (e.g. "Black", "White")
* `hardwareModel` - The device module (e.g. "[N41AP](http://theiphonewiki.com/wiki/N41ap)")
* `modelNumber` - The model number (e.g. "MD636")
* `productType` - The product type or model id (e.g. "iPhone5,1")
* `productVersion` - The iOS version (e.g. "6.1.4")
* `serialNumber` - The device serial number (e.g. "XXXXXXXXXXXX")

There is more data that could have been retrieved from the device, but the properties above seemed
the most reasonable.

### `watch()`

Continuously retrieves an array of all connected iOS devices. Whenever a device is connected or
disconnected, the `'change'` event is emitted.

Returns an `EventEmitter`-based `Handle` instance that contains a `stop()` method to discontinue
tracking devices.

#### Event: `'change'`

Emitted when a device is connected or disconnected.

- `{Array<Object>} devices` - An array of devices

#### Example:

```js
const handle = iosDevice.watch()
    .on('change', console.log);

setTimeout(() => {
    // turn off tracking after 1 minute
    handle.stop();
}, 60000);
```

### `install(udid, appPath)`

Installs an iOS app on the specified device.

* `{String} udid` - The device udid
* `{String} appPath` - The path to the iOS .app

Currently, an `appPath` that begins with `~` is not supported.

The `appPath` must resolve to an iOS .app, not the .ipa file.

### `syslog(udid)`

Relays the syslog from the iOS device.

> Starting with iOS 10, the syslog no longer contains application specific output. If you want
> output for a specific app, then you will need to use a TCP socket. See
> [`forward()`](#forwardudid-port) for more info.

* `{String} udid` - The device udid

Returns a `Handle` instance that contains a `stop()` method to discontinue emitting messages.

> NOTE: `syslog()` only supports USB connected devices. Wi-Fi-only connected devices will not work.

#### Event: `'data'`

Emitted for each line of output. Empty lines are omitted.

- `{String} message` - The log message.

#### Event: 'end'

Emitted when the device is physically disconnected. Note that this does not unregister the internal
callback. You must manually call `handle.stop()` to cleanup.

#### Example:

```js
const handle = iosDevice
	.syslog('<device udid>')
    .on('data', console.log)
    .on('end', () => console.log('End of syslog'));

setTimeout(function () {
	// turn off logging after 1 minute
	handle.stop();
}, 60000);
```

### `forward(udid, port)`

Relays messages from a server running on the device on the specified port.

* `{String} udid` - The device udid
* `{String} port` - The TCP port listening in the iOS app to connect to

Returns a `Handle` instance that contains a `stop()` method to discontinue
emitting messages.

> NOTE: `forward()` only supports USB connected devices. Wi-Fi-only connected devices will not work.

#### Event: `'data'`

Emitted for each line of output. Empty lines are omitted.

- `{String} message` - The log message.

#### Event: 'end'

Emitted when the device is physically disconnected. Note that this does not unregister the internal
callback. You must manually call `handle.stop()` to cleanup.

#### Example:

```js
const handle = iosDevice
	.forward('<device udid>', 1337)
    .on('log', console.log)
    .on('end', () => console.log('End of forward'));

setTimeout(function () {
	// turn off logging after 1 minute
	handle.stop();
}, 60000);
```

## Advanced

### Debug Logging

`node-ios-device` exposes an event emitter that emits debug log messages. This is intended to help
debug issues under the hood. The average user will never need to use this, however it would be
handy when filing a bug.

```js
iosDevice.on('log', msg => console.log(msg));
```

Alternatively, `node-ios-device` uses the amazing [`snooplogg`][2] debug logger where you simply
set the `SNOOPLOGG` environment variable to `node-ios-device` (or `*`) and it will print the debug
log to stdout.

## License

This project is open source under the [Apache Public License v2][1] and is developed by
[Axway, Inc](http://www.axway.com/) and the community. Please read the [`LICENSE`][1] file included
in this distribution for more information.

[1]: https://github.com/appcelerator/node-ios-device/blob/master/LICENSE
[2]: https://www.npmjs.com/package/snooplogg
[3]: https://travis-ci.org/appcelerator/node-ios-device.svg?branch=master
[4]: https://travis-ci.org/appcelerator/node-ios-device
[5]: https://badges.greenkeeper.io/appcelerator/node-ios-device.svg
[6]: https://greenkeeper.io/
