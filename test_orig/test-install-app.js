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

		const { udid } = devices[0];

		const handle = iosDevice.log(udid);
		handle.on('log', console.log);

		console.log('=====================================================');
		console.log('Installing app');
		console.log('=====================================================');

		try {
			await iosDevice.installApp(udid, process.argv.length > 2 ? process.argv[2] : __dirname + '/TestApp.app');
			console.log('=====================================================');
			console.log('App installed successfully');
			console.log('=====================================================');
		} catch (err) {
			handle.stop();
			throw err;
		}
	} catch (err) {
		console.error(err.toString());
		process.exit(1);
	}
})();
