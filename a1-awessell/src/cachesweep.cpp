#include <time.h>
#include <cstdio>
#include <cstdint>
#include <vector>
#include <random>
#include <algorithm>
#include <string>
#include <iostream>

static inline double now_ns() {
    timespec ts; clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1e9 + ts.tv_nsec;
}

// Required branchless version
__attribute__((noinline))
double max_B(const double* a, size_t n) {
    double m = a[0];
    for (size_t i = 1; i < n; ++i) m = (a[i] > m) ? a[i] : m;
    return m;
}

// Optional: 4 independent accumulators to break the dependency chain
__attribute__((noinline))
double max_B4(const double* a, size_t n) {
    double m0 = a[0], m1 = a[0], m2 = a[0], m3 = a[0];
    size_t i = 1;
    for (; i + 3 < n; i += 4) {
        m0 = (a[i]   > m0) ? a[i]   : m0;
        m1 = (a[i+1] > m1) ? a[i+1] : m1;
        m2 = (a[i+2] > m2) ? a[i+2] : m2;
        m3 = (a[i+3] > m3) ? a[i+3] : m3;
    }
    for (; i < n; ++i) m0 = (a[i] > m0) ? a[i] : m0;
    m1 = (m1 > m0) ? m1 : m0;
    m3 = (m3 > m2) ? m3 : m2;
    return (m3 > m1) ? m3 : m1;
}

using Kernel = double (*)(const double*, size_t);
static volatile double g_sink;

// Total elapsed ns for `passes` back-to-back scans of the array
static double time_passes(Kernel k, const double* a, size_t n, long passes) {
    double acc = 0;
    double t0 = now_ns();
    for (long p = 0; p < passes; ++p) {
        const double* q = a;
        asm volatile("" : "+r"(q) :: "memory");   // compiler can't hoist/merge calls
        acc += k(q, n);
    }
    double t1 = now_ns();
    g_sink = acc;                                  // use the result
    return t1 - t0;
}

int main(int argc, char** argv) {
    if (argc != 4) {
        std::cerr << "usage: " << argv[0] << " <B|B4> <reps> <seed>\n";
        return 1;
    }
    std::string variant = argv[1];
    int reps = std::stoi(argv[2]);
    uint64_t seed = std::stoull(argv[3]);

    Kernel k = (variant == "B") ? max_B : (variant == "B4") ? max_B4 : nullptr;
    if (!k) { std::cerr << "unknown variant\n"; return 1; }

    // Allocate and fill the largest array once; smaller sizes use a prefix
    const size_t max_bytes = 1ull << 28;                       // 256 MB
    std::vector<double> a(max_bytes / sizeof(double));
    std::mt19937_64 rng(seed);
    std::uniform_real_distribution<double> d(0.0, 1.0);
    for (auto& x : a) x = d(rng);

    for (size_t bytes = 1ull << 11; bytes <= max_bytes; bytes <<= 1) {   // 2 KB .. 256 MB
        size_t n = bytes / sizeof(double);

        // Verify outside the timed region
        double want = *std::max_element(a.begin(), a.begin() + n);
        if (k(a.data(), n) != want) {
            std::cerr << "wrong result at " << bytes << " bytes\n";
            return 1;
        }

        // Calibrate: double the pass count until one timed batch takes >= 2 ms.
        // This also serves as the warm-up.
        long passes = 1;
        while (time_passes(k, a.data(), n, passes) < 2e6) passes *= 2;

        for (int r = 0; r < reps; ++r) {
            double t = time_passes(k, a.data(), n, passes);
            // variant,bytes,n,passes,rep,time_ns_per_pass
            std::printf("%s,%zu,%zu,%ld,%d,%.3f\n",
                        variant.c_str(), bytes, n, passes, r, t / passes);
        }
    }
    return 0;
}