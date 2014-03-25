all: node_ios_device_v1 node_ios_device_v11 node_ios_device_v12 node_ios_device_v13 node_ios_device_v14

check_env:
	@which node-gyp || (echo "node-gyp not installed, run 'sudo npm install -g node-gyp'" && false)

# Node.js 0.8.x
node_ios_device_v1: check_env
	node-gyp configure --target=0.8.26
	node-gyp build node_ios_device_v1
	mkdir -p out
	cp build/Release/node_ios_device_v1.node out/

# Node.js 0.10.x
node_ios_device_v11: check_env
	node-gyp configure --target=0.10.26
	node-gyp build node_ios_device_v11
	mkdir -p out
	cp build/Release/node_ios_device_v11.node out/

# Node.js 0.11.0 - 0.11.7
node_ios_device_v12: check_env
	node-gyp configure --target=0.11.7
	node-gyp build node_ios_device_v12
	mkdir -p out
	cp build/Release/node_ios_device_v12.node out/

# Node.js 0.11.8 - 0.11.10
node_ios_device_v13: check_env
	node-gyp configure --target=0.11.10
	node-gyp build node_ios_device_v13
	mkdir -p out
	cp build/Release/node_ios_device_v13.node out/

# Node.js 0.11.11 - 0.11.12+
node_ios_device_v14: check_env
	node-gyp configure --target=0.11.12
	node-gyp build node_ios_device_v14
	mkdir -p out
	cp build/Release/node_ios_device_v14.node out/

clean:
	node-gyp clean
	rm -rf out

.PHONY: clean