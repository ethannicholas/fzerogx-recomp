cmake_minimum_required(VERSION 3.20)
project(fzerogx C CXX)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE RelWithDebInfo)
endif()

# Everything that is not specific to F-Zero GX lives in gcn-recomp (a submodule): the
# recompiler, the GameCube runtime, the renderer and the frontends. See its README.
add_subdirectory(gcn-recomp)

gcn_add_game(
  NAME fzerogx
  TITLE "F-Zero GX"
  LABEL "GX Recomp"
  DISC_ID GFZE01
  DOL_SHA1 421c88106697d3275a3fc26fb7a01bf6d816b271
  SYMBOLS ${CMAKE_SOURCE_DIR}/analysis/symbols.txt
  TABLES ${CMAKE_SOURCE_DIR}/recomp
  FPS 60
  # Most of the game is in relocatable modules: the mode modules on the disc, and the main
  # one, fze.main, which the game assembles in memory at boot; the first build takes it
  # from the running game (gcn-recomp/docs/rels.md).
  RELS fze.car_colchg.rel fze.customize.rel fze.interview.rel fze.movie.rel fze.movie_module.rel
       fze.option.rel fze.pilotpoint.rel fze.profile.rel fze.replay.rel fze.sample.rel fze.sel.rel
       fze.story.rel fze.title.rel fze.winning.rel
  REL_RUNTIME fze.main
  REL_SYMBOLS ${CMAKE_SOURCE_DIR}/analysis/rels)
