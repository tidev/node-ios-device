#!/usr/bin/env node
'use strict';

if (process.platform !== 'darwin') {
	console.warn('Unable to build node-ios-device for ' + process.platform);
	process.exit(0);
}

const path = require('path');
const pkgJson = require(path.resolve(__dirname + '/../package.json'));
const targets = Object.keys(pkgJson.binary.targets);
const nodePreGyp = path.resolve(__dirname, '..', 'node_modules', 'node-pre-gyp', 'bin', 'node-pre-gyp');
const actions = [ 'rebuild' ];

if (process.env.npm_lifecycle_event === 'prepare') {
	const argv = JSON.parse(process.env.npm_config_argv);
	const isPublish = argv.cooked.indexOf('publish') !== -1;
	if (isPublish) {
		actions.push('package', 'publish');
	}
}

const spawnSync = require('child_process').spawnSync;
let exitCode = 0;

targets.forEach(function (target) {
	const args = [ nodePreGyp ].concat('--target=' + target, actions);
	console.log('Executing:', process.execPath, args.join(' '));
	const result = spawnSync(process.execPath, args, { stdio: 'inherit' });
	if (result.status) {
		exitCode = 1;
	}
});

process.exit(exitCode);
