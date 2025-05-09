project('mtce', 'c', 'cpp', 
    default_options : ['c_std=c11', 'cpp_std=c++23'], 
    version: '1.0.0'
)

cmake = import('cmake')

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
    'src/bot/main.cpp'
]

# config 
conf_data = configuration_data()
conf_data.set('VERSION', meson.project_version())
conf_data.set('MESON_CXX_COMPILER', meson.get_compiler('cpp').get_id())
conf_data.set('MESON_CXX_COMPILER_VERSION', meson.get_compiler('cpp').version())
conf_data.set('VECTORIZED_BIT_SIZE', get_option('vectorized_bit_size'))
configure_file(input: 'include/build_config.h.in', output: 'build_config.h', configuration: conf_data)

includes = include_directories('include')

# binaries 
mtce_cli = executable('mtce', [common, cli], cpp_pch: [pch], include_directories: includes)

# --- testing & benchmark ---
if get_option('enable_tests')
    gtest_dep = [
        dependency('gtest', main: true, required: false),
        dependency('gmock', main: true, required: false)
    ]

    benchmark_runs = get_option('benchmark_runs')
    
    mtce_test = executable('mtce-test', [common, test], cpp_pch: [pch], include_directories: includes, dependencies: gtest_dep)
    test('gtest tests', mtce_test)

    benchmark('naive-single-thread', mtce_cli, args: [
        '--config', meson.current_source_dir()/'samples/flame.conf', 
        '--in', meson.current_source_dir()/'samples/sample_charm_dataset.txt',
        '--naive-threads', '1',
        '--benchmark', benchmark_runs.to_string()
    ])

    benchmark('naive-multi-thread', mtce_cli, args: [
        '--config', meson.current_source_dir()/'samples/flame.conf', 
        '--in', meson.current_source_dir()/'samples/sample_charm_dataset.txt',
        '--benchmark', benchmark_runs.to_string()
    ])
endif
