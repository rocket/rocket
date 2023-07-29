project('rocket', 'c', 'cpp',
  version: '0.12.0-devel',
  meson_version: '>=1.0',
  default_options: ['cpp_std=c++11'])

with_editor = get_option('editor')
with_examples = get_option('examples')

sdl_dep = dependency('sdl2', required: with_examples)
gl_dep = dependency('gl', required: with_examples)
glu_dep = dependency('glu', required: false)

cc = meson.get_compiler('c')
cpp = meson.get_compiler('cpp')

m_dep = cc.find_library('m', required : false)

# find qt5
qt5 = import('qt5')
qt5_modules = ['Core', 'Gui', 'Widgets', 'Network', 'Xml']
qt5_dep = dependency('qt5', modules: qt5_modules, required: with_editor)
if qt5_dep.found()
  qt5_websockets_dep = dependency('qt5',
    modules: qt5_modules + ['WebSockets'],
    required: get_option('websockets'))
  if qt5_websockets_dep.found()
    qt5_dep = qt5_websockets_dep
    add_project_arguments('-DQT_WEBSOCKETS_LIB', language: 'cpp')
  endif
endif

# find bass
bass_sdk = get_option('bass-sdk')
if bass_sdk == ''
  bass_dep = cpp.find_library('bass', required: false, disabler: true)
else
  bass_inc = include_directories(bass_sdk)
  bass_lib = cpp.find_library('bass', dirs: [
    join_paths(bass_sdk, 'x64'),
    bass_sdk
  ])
  bass_dep = declare_dependency(dependencies: bass_lib,
    include_directories: bass_inc)
endif

if cc.has_function('getaddrinfo', prefix: '#include <netdb.h>')
  add_project_arguments('-DUSE_GETADDRINFO', language: 'c')
endif

if cc.has_function('setsockopt', prefix: '#include <sys/socket.h>')
  add_project_arguments('-DUSE_NODELAY', language: 'c')
endif

if qt5_dep.found()
  subdir('editor')
endif

subdir('lib')

if sdl_dep.found() and gl_dep.found()
  subdir('examples')
endif
