set(FETCHCONTENT_UPDATES_DISCONNECTED_Cppuddle ON)

include(FetchContent)

set(CPPUDDLE_GIT_REPOSITORY "https://github.com/G-071/hpx-kokkos-interopt-WIP")
set(CPPUDDLE_GIT_TAG "master")

message(
  "Fetching from repository: ${CPPUDDLE_GIT_REPOSITORY}, "
  "tag: ${CPPUDDLE_GIT_TAG}"
)

FetchContent_Declare(
  Cppuddle
  GIT_REPOSITORY ${CPPUDDLE_GIT_REPOSITORY}
  GIT_TAG ${CPPUDDLE_GIT_TAG}
  GIT_SHALLOW TRUE
)
FetchContent_GetProperties(Cppuddle)

if(NOT Cppuddle_POPULATED)
  FetchContent_Populate(Cppuddle)
  add_subdirectory(${cppuddle_SOURCE_DIR} ${cppuddle_BINARY_DIR})
  add_library(CPPuddle::buffer_manager INTERFACE IMPORTED)
  target_link_libraries(CPPuddle::buffer_manager INTERFACE buffer_manager)
  add_library(CPPuddle::stream_manager INTERFACE IMPORTED)
  target_link_libraries(CPPuddle::stream_manager INTERFACE stream_manager)
endif()
