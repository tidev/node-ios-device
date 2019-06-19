const iosDevice = require('../ios-device');

iosDevice
	.trackDevices()
	.on('devices', console.log)
	.on('error', err => console.error(err.toString()));
