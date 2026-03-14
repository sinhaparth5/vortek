add_executable(vortek_tests
    main_test.cpp
    kv_store_tests.cpp
    resp_parser_tests.cpp
)

target_link_libraries(vortek_tests
    PRIVATE
        vortek_lib
        Catch2::Catch2
)

include(CTest)
include(Catch)
catch_discover_tests(vortek_tests)
