sources = files('example_bass.cpp')

executable('example_bass', sources,
  link_with: rocket_lib,
  dependencies: [m_dep, sdl_dep, gl_dep, glu_dep, bass_dep])

executable('example_bass-player', sources,
  link_with: rocket_player_lib,
  cpp_args: ['-DSYNC_PLAYER'],
  dependencies: [m_dep, sdl_dep, gl_dep, glu_dep, bass_dep])
