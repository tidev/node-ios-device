# v3.2.0

 * feat: Enable arm64 prebuilt binaries for Apple Silicon now that Xcode supports it. Fixes
   [#75](https://github.com/appcelerator/node-ios-device/issues/75).
 * chore: Updated dependencies.

# v3.1.1 (Jan 5, 2021)

 * chore: Added arm64 prebuildify script for Apple M1 support, but it's currently untested.
 * chore: Updated dependencies.

# v3.1.0 (Dec 2, 2020)

 * chore: Updated dependencies.

# v3.0.0 (Jun 23, 2020)

 * BREAKING CHANGE: Dropped support for Node.js 10.12.0. Please use Node.js 10.13.0 LTS or newer.
 * BREAKING CHANGE: Removed syslog relay API. With iOS 10, syslog no longer returned app specific
   messages. Then in iOS 13, starting the syslog relay service seems to not work reliably.
 * fix: Fix assertion failure where async handle was being deleted before libuv had fully closed
   the handle.
 * fix: Flush pending debug log messages when error is thrown initializing relay.
 * chore: Updated dependencies.

# v2.1.0 (Jun 9, 2020)

 * feat: Added Electron compatibility.
 * chore: Updated dependencies.

# v2.0.2 (Jan 8, 2020)

 * fix: Fixed segfault when copying device value.
 * build: Added support for building on macOS Catalina.
 * chore: Updated dependencies.

# v2.0.1 (Aug 14, 2019)

 * feat: Registered `node-ios-device` bin in the `package.json`.
 * chore: Updated dependencies.

# v2.0.0 (Jul 1, 2019)

 * BREAKING CHANGE: Dropped support for Node.js 8.11 and older.
 * BREAKING CHANGE: Completely new API:
   - `devices()` is now `list()`.
   - `trackDevices()` is now `watch()` and emits a `change` event instead of `devices`.
   - `installApp()` is now `install()`.
   - `log()` has been split up into `syslog()` and `forward()` and emit a `message` event.
 * refactor: Migrated from `nan` to N-API.
