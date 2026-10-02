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

# Keep the legacy executable's shared mapping for older ports. Cyberpunk VR
# Port 0.1.7 uses a separate versioned motion channel and bridge executable.
add_executable(psvr2_toolkit_dsx_bridge_017
  main.cpp
  audio_haptics_capture.cpp
  cyberpunk_config.cpp
  haptics_engine.cpp
  vr_motion_haptics.cpp
  dsx_protocol.cpp)
target_include_directories(psvr2_toolkit_dsx_bridge_017 PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_definitions(psvr2_toolkit_dsx_bridge_017 PRIVATE PSVR2TK_CYBERPUNK_017=1)
target_link_libraries(psvr2_toolkit_dsx_bridge_017 PRIVATE common psvr2_toolkit_capi_loader)

if(WIN32)
  target_link_libraries(psvr2_toolkit_dsx_bridge PRIVATE ws2_32 ole32)
  target_link_libraries(psvr2_toolkit_dsx_bridge_017 PRIVATE ws2_32 ole32)
endif()

option(PSVR2TK_BUILD_DSX_TESTS "Build DSX protocol unit tests" OFF)
if(PSVR2TK_BUILD_DSX_TESTS)
  enable_testing()
  add_executable(psvr2_toolkit_vr_motion_protocol_test vr_motion_protocol_test.cpp)
  add_test(NAME psvr2_toolkit_vr_motion_protocol_test COMMAND psvr2_toolkit_vr_motion_protocol_test)
  add_executable(psvr2_toolkit_machine_release_test machine_release_test.cpp cyberpunk_config.cpp dsx_protocol.cpp)
  target_include_directories(psvr2_toolkit_machine_release_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
  target_link_libraries(psvr2_toolkit_machine_release_test PRIVATE common)
  add_test(NAME psvr2_toolkit_machine_release_test COMMAND psvr2_toolkit_machine_release_test)
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
