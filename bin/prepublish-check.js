#!/usr/bin/env node

/**
 * NPM Prepublish Check Script
 *
 * To test, run:
 *   npm_execpath=/usr/local/lib/node_modules/npm/bin/npm-cli.js npm_lifecycle_event=prepublish node prepublish-check.js
 */

if (process.env.npm_lifecycle_event !== 'prepublish') {
	console.log('This script is meant to be run as an npm prepublish script');
	process.exit(0);
}

var fs = require('fs');
var path = require('path');

var npgDir = path.resolve(__dirname, '../node_modules/node-pre-gyp');
if (!fs.existsSync(npgDir)) {
	console.error('*****************************************************');
	console.error();
	console.error('node-pre-gyp not installed!');
	console.error('You MUST install it using NPM v2.x');
	console.error();
	console.error('*****************************************************');
	process.exit(1);
}

var npgPkgJson = require(path.join(npgDir, 'package.json'));

Object.keys(npgPkgJson.dependencies).forEach(function (name) {
	var p = path.join(npgDir, 'node_modules', name, 'package.json');
	if (!fs.existsSync(p)) {
		console.error('*****************************************************');
		console.error();
		console.error('Dependencies were installed with NPM 3 or newer!');
		console.error('You MUST install dependencies using NPM v2.x');
		console.error();
		console.error('To fix:');
		console.error(' * Delete the node_modules directory');
		console.error(' * Install NPM v2.x');
		console.error(' * Run `npm install`');
		console.error();
		console.error('*****************************************************');
		process.exit(1);
	}
});
