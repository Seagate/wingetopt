# Modifications Copyright 2022 Seagate Technology and/or its Affiliates

# Setting meson version >=0.53 since that was tested and worked as expected on one system inside Seagate
project(
  'wingetopt',
  'c',
  version: '0.96',
  license : ['ISC', 'BSD-3-Clause'],
  meson_version: '>= 0.53',
  default_options: [
    'warning_level=3',
    'default_library=static'
  ],
)

compiler = meson.get_compiler('c')

c_args = []

if compiler.get_id() == 'msvc'
  # -D_CRT_SECURE_NO_WARNINGS is not needed as the code checks flags to switch to MSFT preferred functions as needed-TJE
  if get_option('default_library') == 'shared'
    c_args += [
      '-DBUILDING_WINGETOPT_DLL',
      '-DWINGETOPT_SHARED_LIB',
    ]
  endif
endif

wingetopt_lib = library(
  'wingetopt',
  'src/getopt.c',
  include_directories: include_directories(
    'src',
  ),
  c_args: c_args,
)

wingetopt_dep = declare_dependency(
  link_with: wingetopt_lib,
  include_directories: include_directories(
    'src',
  ),
)
