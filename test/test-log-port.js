const iosDevice = require('../ios-device');

(async () => {
	try {
		const devices = await iosDevice.devices();
		console.log(devices);
		console.log('=====================================================');

		if (!devices.length) {
			console.log('No devices detected');
			return;
		}

		iosDevice
			.log(devices[0].udid, 10571)
			.on('log', console.log)
			.on('error', err => console.error('Error!', err));
	} catch (err) {
		console.error(err.toString());
		process.exit(1);
	}
})();
