add_executable(vortek_tests
    main_test.cpp
    kv_store_tests.cpp
    resp_parser_tests.cpp
    command_tests.cpp
    collection_tests.cpp
    pubsub_tests.cpp
    aof_persistence_tests.cpp
    config_loader_tests.cpp
    compatibility_tests.cpp
)

target_link_libraries(vortek_tests
    PRIVATE
        vortek_lib
        Catch2::Catch2
        vortek_warnings
)

include(CTest)
include(Catch)
catch_discover_tests(vortek_tests)

if(UNIX)
    add_test(
        NAME vortek_signal_shutdown_sigint
        COMMAND ${CMAKE_SOURCE_DIR}/scripts/test_signal_shutdown.sh $<TARGET_FILE:vortek> SIGINT
    )
    add_test(
        NAME vortek_signal_shutdown_sigterm
        COMMAND ${CMAKE_SOURCE_DIR}/scripts/test_signal_shutdown.sh $<TARGET_FILE:vortek> SIGTERM
    )
endif()
