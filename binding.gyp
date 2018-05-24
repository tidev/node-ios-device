{
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
							'inputs': [ '/System/Library/PrivateFrameworks/MobileDevice.framework' ],
							'outputs': [ '<(module_root_dir)/build/MobileDevice.framework' ],
							'action': [ 'cp', '-R', '<@(_inputs)', '<@(_outputs)' ]
						}
					],
					'postbuilds': [
						{
							'postbuild_name': 'Create binding directory',
							'action': [
								'mkdir',
								'-p',
								'<(module_path)'
							]
						},
						{
							'postbuild_name': 'Copy binary into binding directory',
							'action': [
								'cp',
								'<(PRODUCT_DIR)/<(module_name).node',
								'<(module_path)/<(module_name).node'
							]
						}
					]
				}
			]
		}]
	]
}
