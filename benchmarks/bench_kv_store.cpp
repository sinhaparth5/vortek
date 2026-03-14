#include <benchmark/benchmark.h>

#include "core/kv_store.hpp"

using namespace vortek;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static std::string make_key(int64_t i) {
    return "key:" + std::to_string(i);
}

// Pre-populate the store with `n` keys.
static void populate(KvStore& store, int64_t n) {
    for (int64_t i = 0; i < n; ++i)
        store.set(make_key(i), Value{std::to_string(i)});
}

// ---------------------------------------------------------------------------
// SET
// ---------------------------------------------------------------------------

static void BM_KvStore_Set(benchmark::State& state) {
    KvStore store;
    int64_t i = 0;
    for (auto _ : state) {
        store.set(make_key(i % 10000), Value{"value"});
        ++i;
    }
    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_KvStore_Set)->Threads(1)->Threads(4)->Threads(8);

// ---------------------------------------------------------------------------
// SET with TTL
// ---------------------------------------------------------------------------

static void BM_KvStore_Set_WithTTL(benchmark::State& state) {
    KvStore store;
    int64_t i = 0;
    for (auto _ : state) {
        store.set(make_key(i % 10000), Value{"value"},
                  std::chrono::milliseconds{60000});
        ++i;
    }
    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_KvStore_Set_WithTTL)->Threads(1)->Threads(4);

// ---------------------------------------------------------------------------
// GET — cache-hot hit
// ---------------------------------------------------------------------------

static void BM_KvStore_Get_Hit(benchmark::State& state) {
    KvStore store;
    populate(store, 10000);
    int64_t i = 0;
    for (auto _ : state) {
        benchmark::DoNotOptimize(store.get(make_key(i % 10000)));
        ++i;
    }
    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_KvStore_Get_Hit)->Threads(1)->Threads(4)->Threads(8);

// ---------------------------------------------------------------------------
// GET — always miss
// ---------------------------------------------------------------------------

static void BM_KvStore_Get_Miss(benchmark::State& state) {
    KvStore store;
    int64_t i = 0;
    for (auto _ : state) {
        benchmark::DoNotOptimize(store.get("missing:" + std::to_string(i)));
        ++i;
    }
    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_KvStore_Get_Miss)->Threads(1)->Threads(4);

// ---------------------------------------------------------------------------
// EXISTS
// ---------------------------------------------------------------------------

static void BM_KvStore_Exists(benchmark::State& state) {
    KvStore store;
    populate(store, 10000);
    int64_t i = 0;
    for (auto _ : state) {
        benchmark::DoNotOptimize(store.exists(make_key(i % 10000)));
        ++i;
    }
    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_KvStore_Exists)->Threads(1)->Threads(4)->Threads(8);

// ---------------------------------------------------------------------------
// DEL
// ---------------------------------------------------------------------------

static void BM_KvStore_Del(benchmark::State& state) {
    KvStore store;
    int64_t i = 0;
    for (auto _ : state) {
        // Alternate set/del so the key is always present on del.
        const auto key = make_key(i % 1000);
        store.set(key, Value{"v"});
        store.del(key);
        ++i;
    }
    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_KvStore_Del)->Threads(1);
