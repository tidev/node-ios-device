# v2.0.0

 * BREAKING CHANGE: Dropped support for Node.js 8.11 and older.
 * BREAKING CHANGE: Completely new API:
   - `devices()` is now `list()`.
   - `trackDevices()` is now `watch()` and emits a `change` event instead of `devices`.
   - `installApp()` is now `install()`.
   - `log()` has been split up into `syslog()` and `forward()` and emit a `message` event.
 * BREAKING CHANGE: Bumped minimum deployment target to 10.14 due to required libc++ features.
 * refactor: Migrated from `nan` to N-API.
