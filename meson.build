project(
    'sysquery',
    ['c'],
    version : '1.0',
    license : 'GPL-2.0',
    default_options : [
        'c_std=c99',
    ],
)

c_args = [
    '-Wall',
    '-Wextra',
    '-O2',
    '-D_GNU_SOURCE',
]

app_deps = [
    dependency('tinyc'),
]

app_src = [
    'main.c',
    'prc_query.c',
    'query.c',
]

executable(
    meson.project_name(),
    c_args : c_args,
    dependencies : app_deps,
    sources : app_src,
    install : true,
)


