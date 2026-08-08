# Felworld: register this module's unit tests with the core test target.
# src/test/CMakeLists.txt consumes these global properties when the build is
# configured with BUILD_TESTING=ON; otherwise they are inert. Test sources
# live outside src/ so the normal module source glob never compiles them into
# the game binary.
if(TARGET modules)
    set_property(GLOBAL APPEND PROPERTY ACORE_MODULE_TEST_SOURCES
        "${CMAKE_CURRENT_LIST_DIR}/test/BotCommandPrefixTest.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/test/BystanderDistressTest.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/test/LevelPerceptionTest.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/test/PlayerbotsToggleCommandTest.cpp"
    )
endif()
