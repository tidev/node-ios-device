all: node-ios-device node-ios-device-isolate

node-ios-device: clean
	node-gyp configure --target=0.10.21
	node-gyp build node-ios-device
	mkdir -p out
	cp build/Release/node-ios-device.node out/

node-ios-device-isolate: clean
	node-gyp configure --target=0.11.7
	node-gyp build node-ios-device-isolate
	mkdir -p out
	cp build/Release/node-ios-device-isolate.node out/

clean:
	node-gyp clean

.PHONY: clean