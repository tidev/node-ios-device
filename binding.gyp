{
	'conditions': [
		['OS=="mac"',
			{
				'targets': [
					{
						'target_name': 'node_ios_device',
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
					}
				]
			}
		]
	]
}