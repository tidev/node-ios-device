all: node_ios_device_v1 node_ios_device_v11 node_ios_device_v12 node_ios_device_v13

# Node.js 0.8.x
node_ios_device_v1:
	node-gyp configure --target=0.8.26
	node-gyp build node_ios_device_v1
	mkdir -p out
	cp build/Release/node_ios_device_v1.node out/

# Node.js 0.10.x
node_ios_device_v11:
	node-gyp configure --target=0.10.21
	node-gyp build node_ios_device_v11
	mkdir -p out
	cp build/Release/node_ios_device_v11.node out/

# Node.js 0.11.0 - 0.11.7
node_ios_device_v12:
	node-gyp configure --target=0.11.7
	node-gyp build node_ios_device_v12
	mkdir -p out
	cp build/Release/node_ios_device_v12.node out/

# Node.js 0.11.8+
node_ios_device_v13:
	node-gyp configure --target=0.11.10
	node-gyp build node_ios_device_v13
	mkdir -p out
	cp build/Release/node_ios_device_v13.node out/

clean:
	node-gyp clean
	rm -rf out

.PHONY: clean