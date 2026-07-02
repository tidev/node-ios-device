import { IOSDevice } from '../dist/index.mjs';

const device = new IOSDevice();
const devices = device.list();
console.log(devices);
