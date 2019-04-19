const iosDevice = require('../ios-device');

(async () => {
	try {
		const devices = await iosDevice.devices();
		console.log(devices);
	} catch (err) {
		console.error(err.toString());
		process.exit(1);
	}
})();
