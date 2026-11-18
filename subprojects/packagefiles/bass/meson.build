project('bass', 'c',
         version: '2.4.17')

lib_dirs = [meson.current_source_dir()]
if host_machine.system() == 'windows'
  if not ['x86', 'x86_64'].contains(host_machine.cpu_family())
    error('This wrap of bass is for x86 and ARM, and will not work on ' +
          'your system')
  endif
  lib_dirs += [
    meson.current_source_dir() / 'x64',
    meson.current_source_dir() / 'c',
    meson.current_source_dir() / 'c' / 'x64']
elif host_machine.system() == 'linux'
  if not ['aarch64', 'armhf', 'x86', 'x86_64'].contains(host_machine.cpu_family())
    error('This wrap of bass is for x86 and ARM, and will not work on ' +
          'your system')
  endif
  lib_dirs = [meson.current_source_dir() / 'libs' / host_machine.cpu_family()]
elif host_machine.system() != 'darwin'
  error('This wrap of bass is for Linux, Windows and macOS and will not ' +
        'work on your system')
endif

cc = meson.get_compiler('c')
bass_dep = declare_dependency(
dependencies : cc.find_library('bass', dirs : lib_dirs),
include_directories : include_directories('.'))

meson.override_dependency('bass', bass_dep)
