{
	'conditions': [
		['OS=="mac"', {
			'targets': [
				{
					'target_name': 'node_ios_device',
					'defines': [
						"NODE_IOS_DEVICE_VERSION=\"<!(node -e \"console.log(require(\'./package.json\').version)\")\"",
						"NODE_IOS_DEVICE_URL=\"<!(node -e \"console.log(require(\'./package.json\').homepage)\")\""
					],
					'sources': [
						'src/device.cpp',
						'src/device.h',
						'src/device-interface.cpp',
						'src/device-interface.h',
						'src/deviceman.cpp',
						'src/deviceman.h',
						'src/mobiledevice.h',
						'src/node-ios-device.cpp',
						'src/node-ios-device.h',
						'src/relay.cpp',
						'src/relay.h'
					],
					'libraries': [
						'/System/Library/Frameworks/CoreFoundation.framework',
						'MobileDevice.framework'
					],
					'mac_framework_dirs': [
						'<(module_root_dir)/build'
					],
					'include_dirs': [
						'<!(node -e "require(\'napi-macros\')")'
					],
					'cflags': [
						'-Wl,-whole-archive -Wl,--no-whole-archive'
					],
					'cflags!': [
						'-fno-exceptions'
					],
					'cflags_cc!': [
						'-fno-exceptions'
					],
					'actions': [
						{
							'action_name': 'copy_mobiledevice',
							'inputs': [ '/System/Library/PrivateFrameworks/MobileDevice.framework' ],
							'outputs': [ '<(module_root_dir)/build/MobileDevice.framework' ],
							'action': [ 'cp', '-R', '<@(_inputs)', '<@(_outputs)' ]
						}
					],
					'xcode_settings': {
						'OTHER_CPLUSPLUSFLAGS' : [ '-std=c++11', '-stdlib=libc++' ],
						'OTHER_LDFLAGS': [ '-stdlib=libc++' ],
						'MACOSX_DEPLOYMENT_TARGET': '10.11',
						'GCC_ENABLE_CPP_EXCEPTIONS': 'YES'
					}
				}
			]
		}, {
			'targets': [
				{
					'target_name': 'node_ios_device',
					'type': 'none'
				}
			]
		}]
	]
}
