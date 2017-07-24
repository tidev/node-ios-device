#!/usr/bin/env node

if (process.platform !== 'darwin') {
	console.log('Skipping for ' + process.platform + '\n');
	process.exit(0);
}

var spawnSync = require('child_process').spawnSync;
var args = process.argv.slice(2);
var cmd = args.shift();

var result = spawnSync(cmd, args, { stdio: 'inherit' });
process.exit(result.status);
