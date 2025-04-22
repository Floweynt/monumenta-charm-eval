project('mtce', 'c', 'cpp', 
    default_options : ['c_std=c11', 'cpp_std=c++23'], 
    version: '1.0.0'
)

pch = [
    'pch/pch.h'
]

common = [
    'src/eval_naive.cpp',
]

cli = [
    'src/main.cpp',
    'src/cli.cpp'
]

test = [
    'src/test_naive.cpp'
]

mtce = executable('mtce', [common, cli], cpp_pch: [pch])
mtce_test = executable('mtce-test', [common, test], cpp_pch: [pch])

