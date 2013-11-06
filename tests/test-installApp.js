var iosDevice = require('../ios-device');

iosDevice.devices(function (err, devices) {
	console.log(devices);

	if (devices.length) {
		iosDevice.installApp(devices[0].udid, '/Users/chris/Desktop/testapp2.app', function (err) {
			if (err) {
				console.error('ERROR!!!');
				console.error(err);
			} else {
				console.log('app installed');
			}
		});
	}
});
