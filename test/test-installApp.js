var iosDevice = require('../ios-device');

iosDevice.devices(function (err, devices) {
	if (err) {
		console.error(err.toString());
		process.exit(1);
	}

	console.log(devices);

	if (!devices.length) {
		console.log('No devices detected');
		return;
	}

	var stopLog = iosDevice.log(devices[0].udid, function (msg) {
		console.log('[' + device[0].udid + ']  ' + msg);
	});

	console.log('=====================================================');
	console.log('Installing app');
	console.log('=====================================================');
	iosDevice.installApp(devices[0].udid, process.argv.length > 2 ? process.argv[2] : __dirname + '/TestApp.app', function (err) {
		if (err) {
			console.error('ERROR!!!');
			console.error(err);
			stopLog();
			process.exit(1);
		} else {
			console.log('=====================================================');
			console.log('App installed successfully');
			console.log('=====================================================');
		}
	});
});
