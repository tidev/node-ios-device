import { EventEmitter } from 'node:events';
import { readdirSync, statSync } from 'node:fs';
import { createRequire } from 'node:module';
import { dirname, join, resolve } from 'node:path';
import { fileURLToPath } from 'node:url';
import snooplogg from 'snooplogg';

const logger = snooplogg('node-ios-device');
const nss = {};
const extRE = /\.node$/;

function findBinding(): string {
	const __dirname = dirname(fileURLToPath(import.meta.url));

	for (const type of ['Release', 'Debug'] as const) {
		try {
			for (const name of readdirSync(join(__dirname, 'build', type))) {
				if (extRE.test(name)) {
					return resolve(__dirname, 'build', type, name);
				}
			}
		} catch {}
	}

	try {
		for (const target of readdirSync(join(__dirname, 'prebuilds'))) {
			const [platform, arch] = target.split('-');
			if (arch === process.arch && platform === process.platform) {
				for (const binding of readdirSync(join(__dirname, 'prebuilds', target))) {
					if (extRE.test(binding)) {
						return resolve(__dirname, 'prebuilds', target, binding);
					}
				}
			}
		}
	} catch {}

	throw new Error('Unable to find node-ios-device binding');
}

const req = createRequire(import.meta.url);
const binding = req(findBinding());

export class ForwardHandle extends EventEmitter {
	emitFn: (event: string, ...args: any[]) => void;
	udid: string;
	port: number;

	constructor(udid: string, port: number) {
		super();
		this.emitFn = this.emit.bind(this);
		this.udid = udid;
		this.port = port;
		binding.startForward(udid, port, this.emitFn);
	}

	stop() {
		binding.stopForward(this.udid, this.port, this.emitFn);
	}
}

export class WatchHandle extends EventEmitter {
	emitFn: (event: string, ...args: any[]) => void;

	constructor() {
		super();
		this.emitFn = this.emit.bind(this);
		setImmediate(() => binding.watch(this.emitFn));
	}

	stop() {
		binding.unwatch(this.emitFn);
	}
}

export type IOSDevice = {
	udid: string;
	interfaces: ('USB' | 'Wi-Fi')[];
	name: string;
	buildVersion: string;
	cpuArchitecture: string;
	deviceClass: string;
	deviceColor: 'White' | 'Black' | 'Silver' | 'Gold' | 'Rose Gold' | 'Jet Black';
	hardwareModel: string;
	modelNumber: string;
	productType: string;
	productVersion: string;
	serialNumber: string;
	trustedHostAttached: boolean;
};

export class NodeIOSDevice extends EventEmitter {
	constructor() {
		super();

		// init node-ios-device's debug logging and device manager
		// note: this is synchronous
		binding.init((ns, msg) => {
			this.emit('log', msg);
			if (ns) {
				(nss[ns] || (nss[ns] = logger(ns))).log(msg);
			} else {
				logger.log(msg);
			}
		});
	}

	/**
	 * Connects to a server running on the iOS device and relays the data.
	 *
	 * @param {String} udid - The device udid to install the app to.
	 * @param {Number} port - The port number to connect to and forward messages from.
	 * @returns {Promise<EventEmitter>} Resolves a handle to wire up listeners and stop watching.
	 * @emits {data} Emits a buffer containing the data.
	 * @emits {end} Emits when the device has been disconnected.
	 */
	forward(udid, port): ForwardHandle {
		if (!udid || typeof udid !== 'string') {
			throw new TypeError('Expected udid to be a non-empty string');
		}

		if (!port || typeof port !== 'number') {
			throw new TypeError('Expected port to be a number');
		}

		return new ForwardHandle(udid, port);
	}

	/**
	 * Installs an iOS app on the specified device.
	 *
	 * @param {String} udid - The device udid to install the app to.
	 * @param {String} appPath - The path to iOS .app directory to install.
	 */
	install(udid: string, appPath: string) {
		if (!udid || typeof udid !== 'string') {
			throw new TypeError('Expected udid to be a non-empty string');
		}

		if (!appPath || typeof appPath !== 'string') {
			throw new TypeError('Expected app path to be a non-empty string');
		}

		appPath = resolve(appPath);

		try {
			statSync(appPath);
		} catch (e) {
			throw new Error(`App not found: ${appPath}`);
		}

		try {
			if (!statSync(join(appPath, 'PkgInfo')).isFile()) {
				throw new Error();
			}
		} catch {
			throw new Error(`Invalid app: ${appPath}`);
		}

		binding.install(udid, appPath);
	}

	/**
	 * Returns a list of all connected iOS devices.
	 *
	 * @returns {Array.<Object>}
	 */
	list(): IOSDevice[] {
		return binding.list();
	}

	/**
	 * Watches a key for changes to subkeys and values.
	 *
	 * @returns {EventEmitter} The handle to wire up listeners and stop watching.
	 * @emits {change} Emits an array of device objects.
	 */
	watch() {
		return new WatchHandle();
	}
}

/**
 * The `node-ios-device` API and debug log emitter.
 *
 * @type {NodeIOSDevice}
 * @emits {log} Emits a debug log message.
 */
export const nodeIOSDevice = new NodeIOSDevice();
export default nodeIOSDevice;
