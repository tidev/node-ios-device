#!/usr/bin/env node
'use strict';

if (process.platform !== 'darwin') {
	console.warn('Unable to build node-ios-device for ' + process.platform);
	process.exit(0);
}

const path = require('path');
const pkgJson = require(path.resolve(__dirname + '/../package.json'));
const targets = Object.keys(pkgJson.binary.targets);
const nodePreGyp = path.resolve(__dirname, '..', 'node_modules/.bin/node-pre-gyp');
const archs = ['x86_64', 'arm64'];
let actions = process.argv.slice(2); // pass in args for what to do

let isPublish = false;

// Determine if we got invoked via an `npm prepare` through `npm publish`
if (process.env.npm_lifecycle_event === 'prepare') {
	if (process.env.npm_command) { // npm v7+
		isPublish = process.env.npm_command === 'publish';
	} else if (process.env.npm_config_argv) { // npm v6-
		const argv = JSON.parse(process.env.npm_config_argv);
		isPublish = argv.cooked.indexOf('publish') !== -1;
	} else { // future-proof if npm changes again
		console.error('Unable to determine if `npm prepare` was called indirectly via `npm publish`. Has npm changed?');
		process.exit(1);
	}

	if (isPublish) {
		actions.push('package');
	}
}
if (actions.length === 0) { // with no args, assume rebuild
	actions.push('rebuild');
} else if (actions.includes('publish')) {
	// if we explicitly said to publish, don't pass that to node-pre-gyp anymore
	// instead invoke our script
	isPublish = true;
	actions = actions.filter(a => a !== 'publish');
}

const spawnSync = require('child_process').spawnSync;
let exitCode = 0;
actions.forEach(action => {
	targets.forEach(target => {
		archs.forEach(arch => {
			const args = [ nodePreGyp ].concat('--target=' + target, '--target_arch=' + arch, action);
			console.log('Executing:', process.execPath, args.join(' '));
			const result = spawnSync(process.execPath, args, { stdio: 'inherit' });
			if (result.status) {
				exitCode = 1;
			}
		});
	});
});

if (exitCode === 0 && isPublish) {
	// need to use our own publish script!
	const result = spawnSync(process.execPath, [ path.resolve(__dirname, 'publish.js') ], { stdio: 'inherit' });
	if (result.status) {
		exitCode = 1;
	}
}

process.exit(exitCode);
