const fs = require('fs');
const iosDevice = require('../src/index');
const path = require('path');
const { expect } = require('chai');
const { spawnSync } = require('child_process');
const appPath = path.resolve(__dirname, 'TestApp', 'build', 'Release-iphoneos', 'TestApp.app');
let _it = it;

const appCompiled = () => (fs.existsSync(path.join(appPath, 'PkgInfo')) && fs.existsSync(path.join(appPath, 'TestApp')));
if (!appCompiled()) {
	try {
		if (!iosDevice.list().length) {
			throw new Error();
		}

		const { status, stdout } = spawnSync('xcodebuild', [ 'clean', 'build' ], {
			cwd: path.resolve(__dirname, 'TestApp')
		});

		if (status || !/BUILD SUCCEEDED/.test(stdout.toString()) || !appCompiled()) {
			throw new Error();
		}
	} catch (e) {
		_it = it.skip;
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
				'serialNumber'
			]);
			expect(device.udid).to.be.a('string');
			expect(device.udid).to.have.lengthOf(40);
			expect(device.interfaces).to.be.an('array');
			expect(device.interfaces).to.include.any.members([ 'USB', 'Wi-Fi' ]);
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
});

describe('install()', () => {
	_it('should install the test app', () => {
		const devices = iosDevice.list();
		if (devices.length) {
			iosDevice.install(devices[0].udid, appPath);
		}
	});
});

describe('forward()', () => {
});

describe('syslog()', () => {
});
