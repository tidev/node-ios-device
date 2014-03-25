var iosDevice = require('../ios-device');

iosDevice.devices(function (err, devices) {
	console.log(devices);

	if (devices.length) {
		iosDevice.openURL(devices[0].udid, 'http://www.google.com');
	}
});
