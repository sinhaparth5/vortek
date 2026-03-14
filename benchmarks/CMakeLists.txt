add_executable(vortek_bench
    bench_kv_store.cpp
    bench_resp_parser.cpp
    bench_dispatcher.cpp
)

target_link_libraries(vortek_bench
    PRIVATE
        vortek_lib
        vortek_benchmark
        benchmark::benchmark_main
        vortek_warnings
)
