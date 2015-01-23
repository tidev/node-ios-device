{
	'target_defaults': {
		'sources': [
			'src/ios-device.cpp',
			'src/mobiledevice.h'
		],
		'libraries': [
			'/System/Library/Frameworks/CoreFoundation.framework',
			'/System/Library/PrivateFrameworks/MobileDevice.framework'
		],
		'mac_framework_dirs': [
			'/System/Library/PrivateFrameworks'
		],
		'include_dirs': [
			'<!(node -e "require(\'nan\')")'
		]
	},
	'conditions': [
		['OS=="mac"',
			{
				'targets': [
					{
						'target_name': 'node_ios_device_v1'
					},
					{
						'target_name': 'node_ios_device_v11'
					},
					{
						'target_name': 'node_ios_device_v12'
					},
					{
						'target_name': 'node_ios_device_v13'
					},
					{
						'target_name': 'node_ios_device_v14'
					},
					{
						'target_name': 'node_ios_device_v42'
					}
				]
			}
		]
	]
}
