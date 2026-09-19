
#include <time.h>
#include <climits>
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

static void merge(int* a, int* tmp, size_t lo, size_t mid, size_t hi) {
    size_t i = lo, j = mid, k = 0;
    while (i < mid && j < hi) tmp[k++] = (a[j] < a[i]) ? a[j++] : a[i++];   // ties stay left: stable
    while (i < mid) tmp[k++] = a[i++];
    while (j < hi)  tmp[k++] = a[j++];
    std::memcpy(a + lo, tmp, (hi - lo) * sizeof(int));
}

static void msort_alloc(int* a, size_t lo, size_t hi) {
    if (hi - lo < 2) return;
    size_t mid = lo + (hi - lo) / 2;
    msort_alloc(a, lo, mid);
    msort_alloc(a, mid, hi);
    int* tmp = (int*)std::malloc((hi - lo) * sizeof(int));
    if (!tmp) { std::fprintf(stderr, "malloc failed\n"); std::exit(1); }
    merge(a, tmp, lo, mid, hi);
    std::free(tmp);
}

static void msort_shared(int* a, int* tmp, size_t lo, size_t hi) {
    if (hi - lo < 2) return;
    size_t mid = lo + (hi - lo) / 2;
    msort_shared(a, tmp, lo, mid);
    msort_shared(a, tmp, mid, hi);
    merge(a, tmp + lo, lo, mid, hi);
}

static int cmp_int(const void* pa, const void* pb) {
    int x = *(const int*)pa, y = *(const int*)pb;
    return (x > y) - (x < y);
}

enum Variant { ALLOC = 0, SHARED = 1, STDSORT = 2, QSORT = 3 };

static void run_variant(Variant v, int* a, int* tmp, size_t n) {
    switch (v) {
        case ALLOC:   msort_alloc(a, 0, n); break;
        case SHARED:  msort_shared(a, tmp, 0, n); break;
        case STDSORT: std::sort(a, a + n); break;
        case QSORT:   std::qsort(a, n, sizeof(int), cmp_int); break;
    }
}

static void fill_input(std::vector<int>& a, char order, uint64_t seed) {
    size_t n = a.size();
    if (order == 'R') {
        std::mt19937_64 rng(seed);
        std::uniform_int_distribution<int> d(0, INT_MAX);
        for (auto& x : a) x = d(rng);
    } else if (order == 'A') {
        for (size_t i = 0; i < n; ++i) a[i] = (int)i;
    } else if (order == 'D') {
        for (size_t i = 0; i < n; ++i) a[i] = (int)(n - 1 - i);
    } else {
        std::fill(a.begin(), a.end(), 42);
    }
}

static bool self_test() {
    std::mt19937_64 rng(7);
    for (size_t n = 0; n <= 70; ++n) {
        for (int range : {2, 5, 1000000}) {
            std::vector<int> a(n), ref;
            for (auto& x : a) x = (int)(rng() % range);
            ref = a;
            std::stable_sort(ref.begin(), ref.end());
            std::vector<int> b = a, tmp(n + 1);
            msort_alloc(b.data(), 0, n);
            if (b != ref) { std::cerr << "self-test: msort_alloc failed, n=" << n << "\n"; return false; }
            b = a;
            msort_shared(b.data(), tmp.data(), 0, n);
            if (b != ref) { std::cerr << "self-test: msort_shared failed, n=" << n << "\n"; return false; }
        }
    }
    return true;
}

int main(int argc, char** argv) {
    if (argc != 6) {
        std::cerr << "usage: " << argv[0] << " <alloc|shared|stdsort|qsort> <R|A|D|E> <n> <reps> <seed>\n";
        return 1;
    }
    const std::string vs = argv[1];
    const char order = argv[2][0];
    const size_t n = std::stoull(argv[3]);
    const int reps = std::stoi(argv[4]);
    const uint64_t seed = std::stoull(argv[5]);

    Variant v;
    if      (vs == "alloc")   v = ALLOC;
    else if (vs == "shared")  v = SHARED;
    else if (vs == "stdsort") v = STDSORT;
    else if (vs == "qsort")   v = QSORT;
    else { std::cerr << "unknown variant\n"; return 1; }
    if (order != 'R' && order != 'A' && order != 'D' && order != 'E') { std::cerr << "unknown input\n"; return 1; }

    if (!self_test()) return 1;

    std::vector<int> orig(n), work(n), ref, tmp;
    fill_input(orig, order, seed);

    ref = orig;
    std::stable_sort(ref.begin(), ref.end());

    if (v == SHARED) {
        tmp.assign(n, 1);
    }

    work = orig;
    run_variant(v, work.data(), tmp.data(), n);
    if (work != ref) { std::cerr << "verification failed (warm-up)\n"; return 1; }

    for (int r = 0; r < reps; ++r) {
        std::memcpy(work.data(), orig.data(), n * sizeof(int));   // restore, untimed
        double t0 = now_ns();
        run_variant(v, work.data(), tmp.data(), n);
        double t1 = now_ns();
        g_sink = n ? work[n / 2] : 0;                              // use the result
        if (work != ref) { std::cerr << "verification failed (rep " << r << ")\n"; return 1; }
        std::printf("%s,%c,%zu,%d,%.0f\n", vs.c_str(), order, n, r, t1 - t0);
    }
    return 0;
}