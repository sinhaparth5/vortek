#include <benchmark/benchmark.h>

#include "protocol/resp_parser.hpp"
#include "protocol/resp_serializer.hpp"

using namespace vortek;

// Pre-serialise wire frames once so we measure only parsing, not formatting.

static const std::string kSimpleString  = "+OK\r\n";
static const std::string kError         = "-ERR something went wrong\r\n";
static const std::string kInteger       = ":12345\r\n";
static const std::string kBulkString    = "$5\r\nhello\r\n";
static const std::string kNullBulk      = "$-1\r\n";

// PING  → *1\r\n$4\r\nPING\r\n
static const std::string kCmdPing = []() {
    RespArray arr;
    arr.elements.push_back(RespBulkString{"PING"});
    return serialize(arr);
}();

// SET key value  → *3\r\n$3\r\nSET\r\n$3\r\nkey\r\n$5\r\nvalue\r\n
static const std::string kCmdSet = []() {
    RespArray arr;
    arr.elements.push_back(RespBulkString{"SET"});
    arr.elements.push_back(RespBulkString{"mykey"});
    arr.elements.push_back(RespBulkString{"myval"});
    return serialize(arr);
}();

// GET key
static const std::string kCmdGet = []() {
    RespArray arr;
    arr.elements.push_back(RespBulkString{"GET"});
    arr.elements.push_back(RespBulkString{"mykey"});
    return serialize(arr);
}();

// A large RPUSH with 10 elements
static const std::string kCmdRpush = []() {
    RespArray arr;
    arr.elements.push_back(RespBulkString{"RPUSH"});
    arr.elements.push_back(RespBulkString{"mylist"});
    for (int i = 0; i < 10; ++i)
        arr.elements.push_back(RespBulkString{"element" + std::to_string(i)});
    return serialize(arr);
}();

// ---------------------------------------------------------------------------

template<const std::string* Wire>
static void BM_Parse(benchmark::State& state) {
    for (auto _ : state) {
        RespParser parser;
        auto result = parser.feed(*Wire);
        benchmark::DoNotOptimize(result);
    }
    state.SetBytesProcessed(
        static_cast<int64_t>(state.iterations()) *
        static_cast<int64_t>(Wire->size()));
}

BENCHMARK_TEMPLATE(BM_Parse, &kSimpleString)->Name("BM_Parse/SimpleString");
BENCHMARK_TEMPLATE(BM_Parse, &kError)       ->Name("BM_Parse/Error");
BENCHMARK_TEMPLATE(BM_Parse, &kInteger)     ->Name("BM_Parse/Integer");
BENCHMARK_TEMPLATE(BM_Parse, &kBulkString)  ->Name("BM_Parse/BulkString");
BENCHMARK_TEMPLATE(BM_Parse, &kNullBulk)    ->Name("BM_Parse/NullBulk");
BENCHMARK_TEMPLATE(BM_Parse, &kCmdPing)     ->Name("BM_Parse/CmdPing");
BENCHMARK_TEMPLATE(BM_Parse, &kCmdSet)      ->Name("BM_Parse/CmdSet");
BENCHMARK_TEMPLATE(BM_Parse, &kCmdGet)      ->Name("BM_Parse/CmdGet");
BENCHMARK_TEMPLATE(BM_Parse, &kCmdRpush)    ->Name("BM_Parse/CmdRpush10");

// ---------------------------------------------------------------------------
// Pipeline: feed 100 SET commands in one chunk, drain all

static const std::string kPipeline100 = []() {
    std::string buf;
    for (int i = 0; i < 100; ++i)
        buf += kCmdSet;
    return buf;
}();

static void BM_Parse_Pipeline100(benchmark::State& state) {
    for (auto _ : state) {
        RespParser parser;
        bool       first = true;
        int        count = 0;
        while (true) {
            auto val = parser.feed(first ? std::string_view{kPipeline100} : std::string_view{});
            first    = false;
            if (!val) break;
            benchmark::DoNotOptimize(val);
            ++count;
        }
        benchmark::DoNotOptimize(count);
    }
    state.SetBytesProcessed(
        static_cast<int64_t>(state.iterations()) *
        static_cast<int64_t>(kPipeline100.size()));
}
BENCHMARK(BM_Parse_Pipeline100);
