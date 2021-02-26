{
	'variables': {
		'build_v8_with_gn': 'false',
		'v8_enable_pointer_compression': 'false',
		'v8_enable_31bit_smis_on_64bit_arch': 'false',
		'mobiledevice_framework_location': '/System/Library/PrivateFrameworks/MobileDevice.framework',
		'new_mobiledevice_framework_location': '/Library/Apple/System/Library/PrivateFrameworks/MobileDevice.framework'
	},
	'conditions': [
		['OS=="mac"', {
			'targets': [
				{
					'target_name': 'node_ios_device',
					'sources': [
						'src/device.h',
						'src/device.cpp',
						'src/message.h',
						'src/mobiledevice.h',
						'src/node-ios-device.cpp',
						'src/runloop.h',
						'src/runloop.cpp',
						'src/util.h',
						'src/util.cpp'
					],
					'libraries': [
						'/System/Library/Frameworks/CoreFoundation.framework',
						'MobileDevice.framework'
					],
					'mac_framework_dirs': [
						'<(module_root_dir)/build'
					],
					'include_dirs': [
						'<!(node -e "require(\'nan\')")'
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
						'OTHER_CPLUSPLUSFLAGS' : [ '-std=c++11', '-stdlib=libc++' ],
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
		}]
	]
}
