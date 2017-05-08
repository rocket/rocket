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

moc_files = qt5.preprocess(moc_headers: moc_headers)
executable('editor', sources, moc_files, dependencies: qt5_dep)
