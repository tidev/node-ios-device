{
	'variables': {
		'source_files': [
			'src/ios-device.cpp',
			'src/mobiledevice.h'
		],
		'library_files': [
			'/System/Library/Frameworks/CoreFoundation.framework',
			'/System/Library/PrivateFrameworks/MobileDevice.framework'
		]
	},
	'target_defaults': {
		'mac_framework_dirs': [
			'/System/Library/PrivateFrameworks'
		]
	},
	'conditions': [
		['OS=="mac"',
			{
				'targets': [
					{
						'target_name': 'node_ios_device_v1',
						'sources': ['<@(source_files)'],
						'libraries': ['<@(library_files)']
					},
					{
						'target_name': 'node_ios_device_v11',
						'sources': ['<@(source_files)'],
						'libraries': ['<@(library_files)']
					},
					{
						'target_name': 'node_ios_device_v12',
						'sources': ['<@(source_files)'],
						'libraries': ['<@(library_files)']
					},
					{
						'target_name': 'node_ios_device_v13',
						'sources': ['<@(source_files)'],
						'libraries': ['<@(library_files)']
					}
				]
			}
		]
	]
}