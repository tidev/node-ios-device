const fs = require('fs');
const iosDevice = require('../src/index');
const path = require('path');
const { expect } = require('chai');
const { spawnSync } = require('child_process');
const appPath = path.resolve(__dirname, 'TestApp', 'build', 'Release-iphoneos', 'TestApp.app');

let udid = null;
let devit = it;
let appit = it;
const appCompiled = () => (fs.existsSync(path.join(appPath, 'PkgInfo')) && fs.existsSync(path.join(appPath, 'TestApp')));

try {
	const devices = iosDevice.list();

	if (!devices.length) {
		devit = it.skip;
		console.warn('\nNOTICE: No iOS Devices connected... skipping device tests');
		throw new Error();
	}

	({ udid } = devices[0]);

	if (!appCompiled()) {
		const { status, stdout } = spawnSync('xcodebuild', [ 'clean', 'build' ], {
			cwd: path.resolve(__dirname, 'TestApp')
		});

		if (status || !/BUILD SUCCEEDED/.test(stdout.toString()) || !appCompiled()) {
			throw new Error();
		}
	}
} catch (e) {
	appit = it.skip;
	appit.only = it.only;
	appit.skip = it.skip;
}

describe('devices()', () => {
	it('should get all connected devices', () => {
		const devices = iosDevice.list();
		expect(devices).to.be.an('array');

		for (const device of devices) {
			expect(device).to.be.an('object');
			expect(device).to.have.keys([
				'udid',
				'interfaces',
				'name',
				'buildVersion',
				'cpuArchitecture',
				'deviceClass',
				'deviceColor',
				'hardwareModel',
				'modelNumber',
				'productType',
				'productVersion',
				'serialNumber'
			]);
			expect(device.udid).to.be.a('string');
			expect(device.udid).to.have.lengthOf(40);
			expect(device.interfaces).to.be.an('array');
			expect([ 'USB', 'Wi-Fi' ]).to.include.any.members(device.interfaces);
			expect(device.name).to.be.a('string');
			expect(device.buildVersion).to.be.a('string');
			expect(device.buildVersion).to.not.equal('');
			expect(device.cpuArchitecture).to.be.a('string');
			expect(device.cpuArchitecture).to.not.equal('');
			expect(device.deviceClass).to.be.a('string');
			expect(device.deviceClass).to.not.equal('');
			expect(device.deviceColor).to.be.a('string');
			expect(device.deviceColor).to.not.equal('');
			expect(device.hardwareModel).to.be.a('string');
			expect(device.hardwareModel).to.not.equal('');
			expect(device.modelNumber).to.be.a('string');
			expect(device.modelNumber).to.not.equal('');
			expect(device.productType).to.be.a('string');
			expect(device.productType).to.not.equal('');
			expect(device.productVersion).to.be.a('string');
			expect(device.productVersion).to.not.equal('');
			expect(device.serialNumber).to.be.a('string');
			expect(device.serialNumber).to.not.equal('');
		}
	});
});

describe('watch()', () => {
	it('should watch for devices', async function () {
		this.timeout(10000);
		this.slow(10000);

		await new Promise((resolve, reject) => {
			const handle = iosDevice.watch();
			let timer = setTimeout(() => {
				handle.stop();
				resolve();
			}, 3000);

			handle.on('change', devices => {
				clearTimeout(timer);
				try {
					expect(devices).to.be.an('array');
					resolve();
				} catch (e) {
					handle.stop();
					reject(e);
				}
			})
		});
	});
});

describe('install()', () => {
	it('should error if udid is invalid', () => {
		expect(() => {
			iosDevice.install();
		}).to.throw(TypeError, 'Expected udid to be a non-empty string');

		expect(() => {
			iosDevice.install(1234);
		}).to.throw(TypeError, 'Expected udid to be a non-empty string');
	});

	it('should error if app path is invalid', () => {
		expect(() => {
			iosDevice.install(udid);
		}).to.throw(TypeError, 'Expected app path to be a non-empty string');

		expect(() => {
			iosDevice.install(udid, 123);
		}).to.throw(Error, 'Expected app path to be a non-empty string');

		const p = path.join(__dirname, 'does_not_exist');
		expect(() => {
			iosDevice.install(udid, p);
		}).to.throw(Error, `App not found: ${p}`);

		expect(() => {
			iosDevice.install(udid, __dirname);
		}).to.throw(Error, `Invalid app: ${__dirname}`);
	});

	it('should error if udid device is not connected', () => {
		expect(() => {
			iosDevice.install('foo', appPath);
		}).to.throw(Error, 'Device "foo" not found');
	});

	appit('should install the test app', function () {
		this.timeout(15000);
		this.slow(15000);

		iosDevice.install(udid, appPath);
	});
});

describe('forward()', () => {
	it('should error if udid is invalid', () => {
		expect(() => {
			iosDevice.forward();
		}).to.throw(TypeError, 'Expected udid to be a non-empty string');

		expect(() => {
			iosDevice.forward(1234);
		}).to.throw(TypeError, 'Expected udid to be a non-empty string');
	});

	it('should error if udid device is not connected', () => {
		expect(() => {
			iosDevice.forward('foo');
		}).to.throw(Error, 'Device "foo" not found');
	});

	it('should fail if port is invalid', () => {
		expect(() => {
			iosDevice.forward(udid);
		}).to.throw(Error, 'Expected port to be a number between 1 and 65535');

		expect(() => {
			iosDevice.forward(udid, 'foo');
		}).to.throw(Error, 'Expected port to be a number between 1 and 65535');

		expect(() => {
			iosDevice.forward(udid, 99999);
		}).to.throw(Error, 'Expected port to be a number between 1 and 65535');
	});

	devit('should fail if cannot connect to port', () => {
		expect(() => {
			iosDevice.forward(udid, '23456');
		}).to.throw(Error, 'Failed to connect to port 23456');
	});

	appit.skip('should forward port messages', function () {
		this.timeout(15000);
		this.slow(15000);

		console.log('\u001b[33m');
		console.log('    ************************************************************');
		console.log('    *                                                          *')
		console.log('    *   USER INTERACTION REQUIRED! Please launch the TestApp   *');
		console.log('    *                                                          *')
		console.log('    ************************************************************');
		console.log('\u001b[0m');

		const tryForward = () => new Promise((resolve, reject) => {
			try {
				const forwardHandle = iosDevice.forward(udid, 12345);
				let counter = 0;
				forwardHandle.on('data', msg => {
					console.log(msg);
					try {
						if (counter++ === 3) {
							forwardHandle.stop();
							resolve();
						}
					} catch (e) {
						reject(e);
					}
				});
			} catch (e) {
				setTimeout(() => tryForward().then(resolve, reject), 1000);
			}
		});

		return tryForward();
	});
});

describe('syslog()', () => {
	it('should fail if udid is invalid', () => {
		expect(() => {
			iosDevice.syslog();
		}).to.throw(TypeError, 'Expected udid to be a non-empty string');

		expect(() => {
			iosDevice.syslog(1234);
		}).to.throw(TypeError, 'Expected udid to be a non-empty string');
	});

	it('should error if udid device is not connected', () => {
		expect(() => {
			iosDevice.syslog('foo');
		}).to.throw(Error, 'Device "foo" not found');
	});

	appit.skip('should relay syslog messages', async function () {
		this.timeout(15000);
		this.slow(15000);

		let counter = 0;
		const syslogHandle = iosDevice.syslog(udid);
		syslogHandle.on('data', msg => {
			counter++;
		});

		await new Promise(resolve => setTimeout(resolve, 2000));
		expect(counter).to.be.gt(0);

		const count = counter;
		syslogHandle.stop();

		await new Promise(resolve => setTimeout(resolve, 2000));
		expect(counter).to.equal(count);
	});
});