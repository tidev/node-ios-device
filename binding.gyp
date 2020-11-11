{
	'variables': {
		'v8_enable_pointer_compression': 0,
		'v8_enable_31bit_smis_on_64bit_arch': 0,
		'mobiledevice_framework_location': '/System/Library/PrivateFrameworks/MobileDevice.framework',
		'new_mobiledevice_framework_location': '/Library/Apple/System/Library/PrivateFrameworks/MobileDevice.framework'
	},
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
					'xcode_settings': {
						'OTHER_CPLUSPLUSFLAGS' : [ '-std=c++17', '-stdlib=libc++' ],
						'OTHER_LDFLAGS': [ '-stdlib=libc++' ],
						'MACOSX_DEPLOYMENT_TARGET': '10.11',
						'GCC_ENABLE_CPP_EXCEPTIONS': 'YES'
					},
					'actions': [
						{
							'action_name': 'copy_mobiledevice',
							'inputs': [ ],
							'outputs': [ '<(module_root_dir)/build/' ],
							'action': [ 
								'./copy-framework.sh', '<@(mobiledevice_framework_location)', '<@(new_mobiledevice_framework_location)', '<@(_outputs)'
							 ]
						}
					]
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
