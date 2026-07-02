import { IOSDevice } from '../dist/index.cjs';

const device = new IOSDevice();
const devices = device.list();
console.log(devices);
