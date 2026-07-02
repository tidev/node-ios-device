import { cpSync, existsSync } from 'node:fs';
import { basename, join } from 'node:path';

// MobileDevice.framework can live in two locations. The first is blocked from being accessed
// when building, so we copy it to our build directory. The second location is for updated
// versions of the framework; the first location becomes a symlink to it when it exists.
const MOBILEDEVICE_LOCATIONS = [
	'/System/Library/PrivateFrameworks/MobileDevice.framework',
	'/Library/Apple/System/Library/PrivateFrameworks/MobileDevice.framework',
];

const buildDirectory = process.argv[2];

if (!buildDirectory) {
	console.error('Usage: node copy-framework.js <build-directory>');
	process.exit(1);
}

let source = '';

for (const location of MOBILEDEVICE_LOCATIONS) {
	if (existsSync(location)) {
		source = location;
	}
}

if (!source) {
	console.error('Could not find MobileDevice.framework');
	process.exit(1);
}

cpSync(source, join(buildDirectory, basename(source)), { dereference: true, recursive: true });
