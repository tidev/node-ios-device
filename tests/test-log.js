var iosDevice = require('../ios-device');

iosDevice.devices(function (err, devices) {
	console.log(devices);

	if (devices.length) {
		iosDevice.log(devices[0].udid, function (msg) {
			console.log(msg);
		});
	}
});
