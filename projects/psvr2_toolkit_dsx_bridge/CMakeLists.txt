add_executable(psvr2_toolkit_dsx_bridge
  main.cpp
  audio_haptics_capture.cpp
  audio_haptics_capture.h
  cyberpunk_config.cpp
  cyberpunk_config.h
  haptics_engine.cpp
  haptics_engine.h
  vr_motion_haptics.cpp
  vr_motion_haptics.h
  dsx_protocol.cpp
  dsx_protocol.h
)

target_include_directories(psvr2_toolkit_dsx_bridge PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(psvr2_toolkit_dsx_bridge PRIVATE common psvr2_toolkit_capi_loader)

if(WIN32)
  target_link_libraries(psvr2_toolkit_dsx_bridge PRIVATE ws2_32 ole32)
endif()

option(PSVR2TK_BUILD_DSX_TESTS "Build DSX protocol unit tests" OFF)
if(PSVR2TK_BUILD_DSX_TESTS)
  enable_testing()
  add_executable(psvr2_toolkit_dsx_protocol_test
    protocol_test.cpp
    cyberpunk_config.cpp
    cyberpunk_config.h
    dsx_protocol.cpp
    dsx_protocol.h
  )
  target_include_directories(psvr2_toolkit_dsx_protocol_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
  target_link_libraries(psvr2_toolkit_dsx_protocol_test PRIVATE common)
  add_test(NAME psvr2_toolkit_dsx_protocol_test COMMAND psvr2_toolkit_dsx_protocol_test)
endif()
