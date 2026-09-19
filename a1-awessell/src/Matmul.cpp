
#include <time.h>
#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <algorithm>
#include <iostream>
#include <random>
#include <string>
#include <vector>

static inline double now_ns() {
    timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1e9 + ts.tv_nsec;
}

volatile double g_sink;

#define ARGS const double* __restrict A, const double* __restrict B, \
             double* __restrict C, int M, int K, int N
#define BODY C[(size_t)i * N + j] += A[(size_t)i * K + k] * B[(size_t)k * N + j];

// All six kernels accumulate into C with +=, so C must be zeroed before each call.
__attribute__((noinline)) void mm_ijk(ARGS) { for (int i = 0; i < M; ++i) for (int j = 0; j < N; ++j) for (int k = 0; k < K; ++k) BODY }
__attribute__((noinline)) void mm_ikj(ARGS) { for (int i = 0; i < M; ++i) for (int k = 0; k < K; ++k) for (int j = 0; j < N; ++j) BODY }
__attribute__((noinline)) void mm_jik(ARGS) { for (int j = 0; j < N; ++j) for (int i = 0; i < M; ++i) for (int k = 0; k < K; ++k) BODY }
__attribute__((noinline)) void mm_jki(ARGS) { for (int j = 0; j < N; ++j) for (int k = 0; k < K; ++k) for (int i = 0; i < M; ++i) BODY }
__attribute__((noinline)) void mm_kij(ARGS) { for (int k = 0; k < K; ++k) for (int i = 0; i < M; ++i) for (int j = 0; j < N; ++j) BODY }
__attribute__((noinline)) void mm_kji(ARGS) { for (int k = 0; k < K; ++k) for (int j = 0; j < N; ++j) for (int i = 0; i < M; ++i) BODY }

typedef void (*MMFn)(ARGS);
struct Order { const char* name; MMFn fn; };
static const Order ORDERS[] = {
    {"ijk", mm_ijk}, {"ikj", mm_ikj}, {"jik", mm_jik},
    {"jki", mm_jki}, {"kij", mm_kij}, {"kji", mm_kji},
};

static bool self_test() {
    const int M = 4, K = 3, N = 5;
    std::vector<double> A(M * K), B(K * N), C(M * N), ref(M * N);
    for (int i = 0; i < M; ++i) for (int k = 0; k < K; ++k) A[i * K + k] = i * K + k + 1;
    for (int k = 0; k < K; ++k) for (int j = 0; j < N; ++j) B[k * N + j] = k * N + j + 1;
    for (int i = 0; i < M; ++i)
        for (int j = 0; j < N; ++j) {
            double s = 0;
            for (int k = 0; k < K; ++k) s += A[i * K + k] * B[k * N + j];
            ref[i * N + j] = s;
        }
    for (const Order& o : ORDERS) {
        std::fill(C.begin(), C.end(), 0.0);
        o.fn(A.data(), B.data(), C.data(), M, K, N);
        if (C != ref) { std::cerr << "self-test failed for order " << o.name << "\n"; return false; }
    }
    return true;
}

int main(int argc, char** argv) {
    if (argc != 6) {
        std::cerr << "usage: " << argv[0] << " <M> <K> <N> <reps> <seed>\n";
        return 1;
    }
    const int M = std::stoi(argv[1]), K = std::stoi(argv[2]), N = std::stoi(argv[3]);
    const int reps = std::stoi(argv[4]);
    const uint64_t seed = std::stoull(argv[5]);

    if (!self_test()) return 1;

    std::vector<double> A((size_t)M * K), B((size_t)K * N), C((size_t)M * N), ref((size_t)M * N);
    std::mt19937_64 rng(seed);
    std::uniform_int_distribution<int> d(1, 9);      // random non-zero integers, exact in double
    for (auto& x : A) x = d(rng);
    for (auto& x : B) x = d(rng);

    std::fill(ref.begin(), ref.end(), 0.0);
    mm_ijk(A.data(), B.data(), ref.data(), M, K, N);

    for (const Order& o : ORDERS) {
        // warm-up, untimed; also verifies against the reference
        std::fill(C.begin(), C.end(), 0.0);
        o.fn(A.data(), B.data(), C.data(), M, K, N);
        if (C != ref) { std::cerr << "order " << o.name << " differs from reference\n"; return 1; }

        for (int r = 0; r < reps; ++r) {
            std::fill(C.begin(), C.end(), 0.0);           // restore, untimed
            double t0 = now_ns();
            o.fn(A.data(), B.data(), C.data(), M, K, N);
            double t1 = now_ns();
            g_sink = C[C.size() / 2];                      // use the result
            if (C != ref) { std::cerr << "order " << o.name << " wrong at rep " << r << "\n"; return 1; }
            std::printf("%s,%d,%d,%d,%d,%.0f\n", o.name, M, K, N, r, t1 - t0);
        }
    }
    return 0;
}