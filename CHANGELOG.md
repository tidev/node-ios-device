# v2.0.0 (Jul 1, 2019)

 * BREAKING CHANGE: Dropped support for Node.js 8.11 and older.
 * BREAKING CHANGE: Completely new API:
   - `devices()` is now `list()`.
   - `trackDevices()` is now `watch()` and emits a `change` event instead of `devices`.
   - `installApp()` is now `install()`.
   - `log()` has been split up into `syslog()` and `forward()` and emit a `message` event.
 * refactor: Migrated from `nan` to N-API.
