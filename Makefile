all: node-ios-device node-ios-device-isolate

node-ios-device: clean
	node-gyp configure --target=0.10.21
	node-gyp build node-ios-device
	cp build/Release/node-ios-device.node .

node-ios-device-isolate: clean
	node-gyp configure --target=0.11.7
	node-gyp build node-ios-device-isolate
	cp build/Release/node-ios-device-isolate.node .

clean:
	node-gyp clean

.PHONY: clean