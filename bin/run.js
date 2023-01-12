#!/usr/bin/env node
'use strict';

if (process.platform !== 'darwin') {
	console.log('Skipping for ' + process.platform + '\n');
	process.exit(0);
}

const spawnSync = require('child_process').spawnSync;

spawnSync('patch-package', { stdio: 'inherit' });

const args = process.argv.slice(2);
const cmd = args.shift();

const result = spawnSync(cmd, args, { stdio: 'inherit' });
process.exit(result.status);
