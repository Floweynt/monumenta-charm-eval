project('mtce', 'c', 'cpp', 
    default_options : ['c_std=c11', 'cpp_std=c++23'], 
    version: '1.0.0'
)

# sources  
pch = [
    'include/pch/pch.h'
]

common = [
    'src/common/eval_naive.cpp',
]

cli = [
    'src/cli/main.cpp',
    'src/cli/cli.cpp'
]

test = [
    'src/test/test_naive.cpp'
]

bot = [
]

# config 
conf_data = configuration_data()
conf_data.set('VERSION', meson.project_version())
conf_data.set('MESON_CXX_COMPILER', meson.get_compiler('cpp').get_id())
configure_file(input: 'include/build_config.h.in', output: 'build_config.h', configuration: conf_data)

includes = include_directories('include')

# deps   
gtest_dep = [
    dependency('gtest', main: true, required: false),
    dependency('gmock', main: true, required: false)
]

# binaries 
mtce_cli = executable('mtce', [common, cli], cpp_pch: [pch], include_directories: includes)
mtce_test = executable('mtce-test', [common, test], cpp_pch: [pch], include_directories: includes, dependencies: gtest_dep)

# testing
test('gtest tests', mtce_test)

# benchmark 
benchmark('naive-single-thread', mtce_cli, args: [
    '--config', meson.current_source_dir()/'samples/flame.conf', 
    '--in', meson.current_source_dir()/'samples/sample_charm_dataset.txt',
    '--naive-threads', '1',
    '--benchmark', '100'
])

benchmark('naive-multi-thread', mtce_cli, args: [
    '--config', meson.current_source_dir()/'samples/flame.conf', 
    '--in', meson.current_source_dir()/'samples/sample_charm_dataset.txt',
    '--benchmark', '100'
])
