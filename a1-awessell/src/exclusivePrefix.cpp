#include <time.h>
#include <iostream>
#include <vector>
#include <algorithm>
#include <random>
#include <cstdio>
#include <limits>
#include <type_traits>
#include <string>
#include <cstdint>
#include <cstring>


template <class T> void fill_input(std::vector<T>& a, uint64_t seed = 12345) {
    std::mt19937_64 rng(seed);
    std::uniform_int_distribution<int> dist(0, 10);
    for (auto& x : a) x = (T)dist(rng);
}

template <class T> bool verify(const T* out, const T* orig, size_t n, T total) {
    if (out[0] != 0) return false;
    for (size_t i = 1; i < n; ++i) {
        if (out[i] != out[i-1] + orig[i-1]) return false;
    }
    return out[n-1] + orig[n-1] == total;
}


static inline double now_ns() {
    timespec ts; clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1e9 + ts.tv_nsec;
}



template <class T> __attribute__((noinline)) T prefix_excl(T* a, size_t n) {
    T sum = 0;
    for (size_t i = 0; i < n; ++i) {
        T x = a[i];
        a[i] = sum;
        sum += x;
    }
    return sum;
}

volatile double g_sink;

template <class T, class F> void bench(F kernel, T* a, size_t n, int reps, std::vector<double>& times) {
    g_sink = (double)kernel(a, n);
    for (int i = 0; i < reps; ++i) {
        double t0 = now_ns();
        T m = kernel(a, n);
        double t1 = now_ns();
        g_sink = (double)m;
        times.push_back(t1 - t0);
    }
}


template <class T> int run(const std::string& opt, const std::string& type, size_t n, int reps, uint64_t seed) {

    std::vector<T> orig(n), work(n);
    fill_input(orig, seed);

    std::memcpy(work.data(), orig.data(), n * sizeof(T));
    g_sink = (double)prefix_excl<T>(work.data(), n);

    for (int r = 0; r < reps; ++r) {
        std::memcpy(work.data(), orig.data(), n * sizeof(T));

        double t0 = now_ns();
        T total = prefix_excl<T>(work.data(), n);
        double t1 = now_ns();

        g_sink = (double)total;
        if (!verify(work.data(), orig.data(), n, total)) {
            std::cerr << "Verification failed for " << type << " with n=" << n << std::endl;
            return 1;
        }
        std::printf("%s, %s, %zu, %d, %.0f\n", opt.c_str(), type.c_str(), n, reps, t1-t0);
    }

    

    return 0;

}

int main(int argc, char** argv) {

    if (argc != 6) {
        std::cerr << "Usage: " << argv[0] << " <O0|O2|O3> <int|double> <n> <reps> <seed>" << std::endl;
        return 1;
    }
    std::string opt = argv[1], type = argv[2];
    size_t n = std::stoull(argv[3]);
    int reps = std::stoi(argv[4]);
    uint64_t seed = std::stoull(argv[5]);

    if (type == "int")    return run<int>(opt, type, n, reps, seed);
    if (type == "double") return run<double>(opt, type, n, reps, seed);
    std::cerr << "Unknown type: " << type << std::endl;
    return 1;

}