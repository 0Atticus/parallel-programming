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



enum class Order { Ascending, Descending, Random };

template <class T> void fill_input(std::vector<T>& a, Order order, uint64_t seed = 12345) {
    size_t n = a.size();
    switch(order) {
        case Order::Ascending:
            for (size_t i = 0; i < n; ++i) a[i] = (T)i;
            break;
        case Order::Descending:
            for (size_t i = 0; i < n; ++i) a[i] = (T)(n - 1 - i);
            break;
        case Order::Random: {
            std::mt19937_64 rng(seed);
            if constexpr (std::is_integral_v<T>) {
                std::uniform_int_distribution<T> dist(0, std::numeric_limits<T>::max());
                for (size_t i = 0; i < n; ++i) a[i] = dist(rng);
            } else {
                std::uniform_real_distribution<T> dist(0.0, 1.0);
                for (size_t i = 0; i < n; ++i) a[i] = dist(rng);
            }
            break;
        }
    }
}


static inline double now_ns() {
    timespec ts; clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1e9 + ts.tv_nsec;
}


template <class T> T max_A(const T* a, size_t n) {
    T m = a[0];
    for (size_t i = 1; i < n; ++i) if (a[i] > m) m = a[i];
    return m;
}

template <class T> T max_B(const T* a, size_t n) {
    T m = a[0];
    for (size_t i = 1; i < n; ++i) m = (a[i] > m) ? a[i] : m;
    return m;
}

volatile double g_sink;


template <class T, class F> void bench(F kernel, const T* a, size_t n, int reps, std::vector<double>& times) {
    g_sink = (double)kernel(a, n);
    for (int i = 0; i < reps; ++i) {
        double t0 = now_ns();
        T m = kernel(a, n);
        double t1 = now_ns();
        g_sink = (double)m;
        times.push_back(t1 - t0);
    }
}

template <class T> T expected_max(const std::vector<T>& a, Order order) {
    if (order == Order::Ascending) return a.back();
    if (order == Order::Descending) return a.front();
    return *std::max_element(a.begin(), a.end());
}


template <class T> int run (const std::string& type, const std::string& variant, size_t n, int reps, Order order, char order_c, uint64_t seed) {
    std::vector<T> a(n);
    fill_input(a, order, seed);

    T want = expected_max(a, order);
    if (max_A(a.data(), n) != want || max_B(a.data(), n) != want) {
        std::cerr << "Error: max_A or max_B returned wrong result for type " << type << ", variant " << variant << ", n = " << n << ", order = " << order_c << std::endl;
        return 1;
    }

    std::vector<double> times;
    times.reserve(reps);
    if(variant == "A") bench<T>(max_A<T>, a.data(), n, reps, times);
    else               bench<T>(max_B<T>, a.data(), n, reps, times);
    
    for (int r = 0; r < reps; ++r) {
        std::printf("%s %s %c %zu %.2f\n", type.c_str(), variant.c_str(), order_c, n, times[r]);
    }
    return 0;

}

// args: <A|B> <int|double> <n> <reps> <A|D|R> <seed>
int main(int argc, char** argv){

    if (argc != 7) {
        std::cerr << "Usage: " << argv[0] << " <A|B> <int|double> <n> <reps> <A|D|R> <seed>" << std::endl;
        return 1;
    }

    std::string variant = argv[1], type = argv[2];
    size_t n = std::stoull(argv[3]);
    int reps = std::stoi(argv[4]);
    char order_c = argv[5][0];
    Order order = (order_c == 'A') ? Order::Ascending
                : (order_c == 'D') ? Order::Descending : Order::Random;
    uint64_t seed = std::stoull(argv[6]);

    if (type =="int") return run<int>(type, variant, n, reps, order, order_c, seed);
    if (type == "double") return run<double>(type, variant, n, reps, order, order_c, seed);
    std::cerr << "Error: unknown type " << type << std::endl;
    return 1;



}