project('canister/core', 'cpp',
	version : '1.0.0',
	license : 'proprietary',
	subproject_dir : 'vendor',
	default_options : [
		'warning_level=3',
		'cpp_std=c++20'
	]
)

compiler = meson.get_compiler('cpp')
lib_dir = meson.current_source_dir() + '/lib'
sources = [
	'src/canister.cpp',
	'src/db.cpp',
	'src/decompress.cpp',
	'src/dpkg.cpp',
	'src/http.cpp',
	'src/log.cpp',
	'src/parser.cpp',
	'src/util.cpp'
]

run_command('./configure.sh', check: true)
message('Built vendor dependencies')

executable('core', sources,
	install : true,
	include_directories: [
		include_directories('include'),
		include_directories('/usr/include/postgresql')
	],
	dependencies: [
		compiler.find_library('uws', dirs: lib_dir, required: true),
		compiler.find_library('taopq', dirs: lib_dir, required: true),
		compiler.find_library('curlpp', dirs: lib_dir, required: true),
		compiler.find_library('sentry', dirs: lib_dir, required: true),
		compiler.find_library('validator', dirs: lib_dir, required: true),
		compiler.find_library('zstd', required: true),
		compiler.find_library('bz2', required: true),
		compiler.find_library('libz', required: true),
		compiler.find_library('lzma', required: true),
		compiler.find_library('curl', required: true),
		compiler.find_library('pq', required: true)
	]
)
