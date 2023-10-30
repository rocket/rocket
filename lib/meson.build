sources = files(
  'device.c',
  'track.c'
)

rocket_lib = static_library('rocket', sources)
rocket_player_lib = static_library('rocket-player', sources,
  c_args: ['-DSYNC_PLAYER'])
