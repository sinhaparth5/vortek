#include <benchmark/benchmark.h>

#include "commands/command.hpp"
#include "commands/dispatcher.hpp"
#include "server/pubsub_broker.hpp"
#include "server/server_stats.hpp"

using namespace vortek;

// ---------------------------------------------------------------------------
// Shared fixtures — created once per benchmark process to avoid startup cost.
// ---------------------------------------------------------------------------

namespace {

ServerStats  g_stats;
PubSubBroker g_broker;
Dispatcher   g_dispatcher = Dispatcher::make_default(g_stats, &g_broker);

Command make_cmd(std::vector<std::string> parts) {
    RespArray arr;
    for (auto& p : parts)
        arr.elements.push_back(RespBulkString{std::move(p)});
    auto cmd = parse_command(RespValue{std::move(arr)});
    return *cmd;  // always valid for well-formed input
}

const Command kPing       = make_cmd({"PING"});
const Command kSet        = make_cmd({"SET", "bench:key", "bench:value"});
const Command kGet        = make_cmd({"GET", "bench:key"});
const Command kGetMiss    = make_cmd({"GET", "bench:miss"});
const Command kIncr       = make_cmd({"INCR", "bench:counter"});
const Command kLpush      = make_cmd({"LPUSH", "bench:list", "a", "b", "c"});
const Command kSadd       = make_cmd({"SADD", "bench:set", "m1", "m2", "m3"});
const Command kHset       = make_cmd({"HSET", "bench:hash", "f1", "v1", "f2", "v2"});

}  // namespace

// ---------------------------------------------------------------------------
// PING
// ---------------------------------------------------------------------------

static void BM_Dispatch_Ping(benchmark::State& state) {
    KvStore store;
    for (auto _ : state)
        benchmark::DoNotOptimize(g_dispatcher.dispatch(kPing, store));
    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_Dispatch_Ping);

// ---------------------------------------------------------------------------
// SET
// ---------------------------------------------------------------------------

static void BM_Dispatch_Set(benchmark::State& state) {
    KvStore store;
    for (auto _ : state)
        benchmark::DoNotOptimize(g_dispatcher.dispatch(kSet, store));
    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_Dispatch_Set)->Threads(1)->Threads(4);

// ---------------------------------------------------------------------------
// GET hit / miss
// ---------------------------------------------------------------------------

static void BM_Dispatch_Get_Hit(benchmark::State& state) {
    KvStore store;
    g_dispatcher.dispatch(kSet, store);  // seed the key
    for (auto _ : state)
        benchmark::DoNotOptimize(g_dispatcher.dispatch(kGet, store));
    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_Dispatch_Get_Hit)->Threads(1)->Threads(4)->Threads(8);

static void BM_Dispatch_Get_Miss(benchmark::State& state) {
    KvStore store;
    for (auto _ : state)
        benchmark::DoNotOptimize(g_dispatcher.dispatch(kGetMiss, store));
    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_Dispatch_Get_Miss)->Threads(1)->Threads(4);

// ---------------------------------------------------------------------------
// INCR
// ---------------------------------------------------------------------------

static void BM_Dispatch_Incr(benchmark::State& state) {
    KvStore store;
    for (auto _ : state)
        benchmark::DoNotOptimize(g_dispatcher.dispatch(kIncr, store));
    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_Dispatch_Incr);

// ---------------------------------------------------------------------------
// Collection commands
// ---------------------------------------------------------------------------

static void BM_Dispatch_Lpush(benchmark::State& state) {
    KvStore store;
    for (auto _ : state)
        benchmark::DoNotOptimize(g_dispatcher.dispatch(kLpush, store));
    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_Dispatch_Lpush);

static void BM_Dispatch_Sadd(benchmark::State& state) {
    KvStore store;
    for (auto _ : state)
        benchmark::DoNotOptimize(g_dispatcher.dispatch(kSadd, store));
    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_Dispatch_Sadd);

static void BM_Dispatch_Hset(benchmark::State& state) {
    KvStore store;
    for (auto _ : state)
        benchmark::DoNotOptimize(g_dispatcher.dispatch(kHset, store));
    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_Dispatch_Hset);

// ---------------------------------------------------------------------------
// Publish — measures broker overhead with zero subscribers
// ---------------------------------------------------------------------------

static void BM_Dispatch_Publish_NoSubs(benchmark::State& state) {
    KvStore      store;
    const Command pub = make_cmd({"PUBLISH", "bench:channel", "hello"});
    for (auto _ : state)
        benchmark::DoNotOptimize(g_dispatcher.dispatch(pub, store));
    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_Dispatch_Publish_NoSubs);
