moc_headers = files(
  'mainwindow.h',
  'syncclient.h',
  'syncdocument.h',
  'syncpage.h',
  'synctrack.h',
  'trackview.h'
)

sources = files(
  'editor.cpp',
  'mainwindow.cpp',
  'syncclient.cpp',
  'syncdocument.cpp',
  'syncpage.cpp',
  'trackview.cpp'
)

_extra_args = []
if qt5_websockets_dep.found()
  _extra_args += '-DQT_WEBSOCKETS_LIB'
endif

moc_files = qt5.preprocess(moc_headers: moc_headers, moc_extra_arguments: _extra_args)
executable('editor', sources, moc_files, dependencies: qt5_dep)
