import fs from 'node:fs';
import iosDevice from '../src/index.js';
import path from 'node:path';
import { assert, describe, expect, it } from 'vitest';
import { spawnSync } from 'node:child_process';

const __dirname = import.meta.dirname;
const appPath = path.resolve(__dirname, 'TestApp', 'build', 'Release-iphoneos', 'TestApp.app');

let udid: string | null = null;
let usbUDID: string | null = null;
let wifiUDID: string | null = null;
let devit: typeof it | typeof it.skip = it;
let usbAppIt = it.skip;
let wifiAppIt = it.skip;
let appit: typeof it | typeof it.skip & { only: typeof it.only; skip: typeof it.skip } = it;

try {
	const devices = iosDevice.list();

	if (!devices.length) {
		devit = it.skip;
		throw new Error('NOTICE: No iOS Devices connected... skipping device tests');
	}

	for (const device of devices) {
		if (!udid) {
			udid = device.udid;
		}
		if (!usbUDID && device.interfaces.includes('USB')) {
			usbUDID = device.udid;
		}
		if (!wifiUDID && device.interfaces.includes('Wi-Fi') && !device.interfaces.includes('USB')) {
			wifiUDID = device.udid;
		}
	}

	if (!process.argv.includes('--skip-build-testapp')) {
		console.log('Building TestApp...')
		const { status, stdout, stderr } = spawnSync('xcodebuild', [ 'clean', 'build' ], {
			cwd: path.resolve(__dirname, 'TestApp')
		});

		if (!/BUILD SUCCEEDED/.test(stdout.toString())) {
			throw new Error(`Build TestApp failed (status ${status})\n${stderr.toString()}`);
		}
	}
} catch (e) {
	console.warn('\n' + (e as Error).toString());
	appit = it.skip;
	if (!appit.only) {
		appit.only = it.only;
	}
	if (!appit.skip) {
		appit.skip = it.skip;
	}
}

if (wifiUDID) {
	wifiAppIt = appit;
	if (!wifiAppIt.only) {
		wifiAppIt.only = it.only;
	}
	if (!wifiAppIt.skip) {
		wifiAppIt.skip = it.skip;
	}
}

if (usbUDID) {
	usbAppIt = appit;
	if (!usbAppIt.only) {
		usbAppIt.only = it.only;
	}
	if (!usbAppIt.skip) {
		usbAppIt.skip = it.skip;
	}
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
				'serialNumber',
				'trustedHostAttached'
			]);
			expect(device.udid).to.be.a('string');
			expect(device.udid).to.not.equal('');
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
			expect(device.trustedHostAttached).to.be.a('boolean');
		}
	});
});

describe('watch()', () => {
	it('should watch for devices', async () => {
		await new Promise<void>((resolve, reject) => {
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
					reject(e);
				} finally {
					handle.stop();
				}
			})
		});
	}, 10000);
});

describe('install()', () => {
	it('should error if udid is invalid', () => {
		expect(() => {
			(iosDevice.install as any)();
		}).to.throw(TypeError, 'Expected udid to be a non-empty string');

		expect(() => {
			(iosDevice.install as any)(1234);
		}).to.throw(TypeError, 'Expected udid to be a non-empty string');
	});

	it('should error if app path is invalid', () => {
		expect(() => {
			(iosDevice.install as any)('foo');
		}).to.throw(TypeError, 'Expected app path to be a non-empty string');

		expect(() => {
			iosDevice.install('foo', 123 as any);
		}).to.throw(Error, 'Expected app path to be a non-empty string');

		const p = path.join(__dirname, 'does_not_exist');
		expect(() => {
			iosDevice.install('foo', p as any);
		}).to.throw(Error, `App not found: ${p}`);

		expect(() => {
			iosDevice.install('foo', __dirname);
		}).to.throw(Error, `Invalid app: ${__dirname}`);
	});

	appit('should error if udid device is not connected', () => {
		expect(() => {
			iosDevice.install('foo', appPath);
		}).to.throw(Error, 'Device "foo" not found');
	});

	appit('should install the test app', function () {
		assert(udid);
		iosDevice.install(udid, appPath);
	}, 15000);
});

describe('forward()', () => {
	it('should error if udid is invalid', () => {
		expect(() => {
			(iosDevice.forward as any)();
		}).to.throw(TypeError, 'Expected udid to be a non-empty string');

		expect(() => {
			(iosDevice.forward as any)(1234);
		}).to.throw(TypeError, 'Expected udid to be a non-empty string');
	});

	it('should error if udid device is not connected', () => {
		expect(() => {
			(iosDevice.forward as any)('foo');
		}).to.throw(Error, 'Expected port to be a number');
	});

	usbAppIt('should fail if port is invalid', () => {
		expect(() => {
			iosDevice.forward(udid, 123 as any);
		}).to.throw(Error, 'Expected port to be a number between 1 and 65535');

		expect(() => {
			iosDevice.forward(udid, 'foo' as any);
		}).to.throw(Error, 'Expected port to be a number between 1 and 65535');

		expect(() => {
			iosDevice.forward(udid, 99999 as any);
		}).to.throw(Error, 'Expected port to be a number between 1 and 65535');
	});

	wifiAppIt('should error trying to forward on Wi-Fi only device', () => {
		expect(() => {
			iosDevice.forward(wifiUDID, 12345 as any);
		}).to.throw(Error, 'forward requires a USB connected iOS device');
	});

	usbAppIt('should fail if cannot connect to port', () => {
		expect(() => {
			iosDevice.forward(udid, '23456' as any);
		}).to.throw(Error, 'Failed to connect to port 23456');
	});

	usbAppIt('should forward port messages', () => {
		console.log('\u001b[33m');
		console.log('    ************************************************************');
		console.log('    *                                                          *')
		console.log('    *   USER INTERACTION REQUIRED! Please launch the TestApp   *');
		console.log('    *                                                          *')
		console.log('    ************************************************************');
		console.log('\u001b[0m');

		let timer: NodeJS.Timeout | null = null;
		const tryForward = () => new Promise<void>((resolve, reject) => {
			try {
				const forwardHandle = iosDevice.forward(usbUDID, 12345);
				let counter = 0;
				forwardHandle.on('data', msg => {
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
				timer = setTimeout(() => tryForward().then(resolve, reject), 1000);
			}
		});

		return Promise.race([
			tryForward(),
			new Promise<void>(resolve => setTimeout(() => {
				if (timer) {
					clearTimeout(timer);
				}
				resolve();
			}, 10000))
		]);
	}, 15000);
});
