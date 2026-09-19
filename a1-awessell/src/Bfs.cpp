
#include <time.h>
#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <algorithm>
#include <iostream>
#include <numeric>
#include <random>
#include <string>
#include <vector>

static inline double now_ns() {
    timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1e9 + ts.tv_nsec;
}

volatile double g_sink;

struct Graph {
    int n = 0;
    long m = 0;                     
    std::vector<long> offsets;      
    std::vector<int> adj;           
};
static bool read_mtx(const char* path, Graph& g, bool& symmetric_out) {
    FILE* f = std::fopen(path, "r");
    if (!f) { std::fprintf(stderr, "cannot open %s\n", path); return false; }

    char line[4096];
    if (!std::fgets(line, sizeof line, f) || std::strncmp(line, "%%MatrixMarket", 14) != 0) {
        std::fprintf(stderr, "%s: missing %%%%MatrixMarket header\n", path);
        std::fclose(f);
        return false;
    }
    const bool symmetric = std::strstr(line, "symmetric") != nullptr;
    symmetric_out = symmetric;

    long rows = 0, cols = 0, nnz = 0;
    bool got_size = false;
    while (std::fgets(line, sizeof line, f)) {
        if (line[0] == '%') continue;
        if (std::sscanf(line, "%ld %ld %ld", &rows, &cols, &nnz) == 3) { got_size = true; break; }
        break;
    }
    if (!got_size) { std::fprintf(stderr, "%s: bad size line\n", path); std::fclose(f); return false; }

    const int n = (int)std::max(rows, cols);
    std::vector<int> eu, ev;
    eu.reserve(nnz);
    ev.reserve(nnz);
    for (long e = 0; e < nnz; ++e) {
        int u, v;
        if (std::fscanf(f, "%d %d%*[^\n]", &u, &v) != 2) {
            std::fprintf(stderr, "%s: truncated at entry %ld of %ld\n", path, e, nnz);
            std::fclose(f);
            return false;
        }
        --u; --v;                                   
        if (u < 0 || v < 0 || u >= n || v >= n) { std::fprintf(stderr, "%s: index out of range\n", path); std::fclose(f); return false; }
        if (u == v) continue;                       
        eu.push_back(u);
        ev.push_back(v);
    }
    std::fclose(f);

    g.n = n;
    g.offsets.assign((size_t)n + 1, 0);
    for (size_t e = 0; e < eu.size(); ++e) {
        g.offsets[eu[e] + 1]++;
        if (symmetric) g.offsets[ev[e] + 1]++;
    }
    for (int i = 0; i < n; ++i) g.offsets[i + 1] += g.offsets[i];
    g.m = g.offsets[n];

    g.adj.assign((size_t)g.m, 0);
    std::vector<long> pos(g.offsets.begin(), g.offsets.end() - 1);   
    for (size_t e = 0; e < eu.size(); ++e) {
        g.adj[pos[eu[e]]++] = ev[e];
        if (symmetric) g.adj[pos[ev[e]]++] = eu[e];
    }
    return true;
}

struct Stats {
    long edges = 0;     
    long reached = 1;   
    int levels = 0;     
};

static Stats bfs(const Graph& g, int src, int* dist, int* cur, int* nxt, std::vector<long>& fsz) {
    const long* off = g.offsets.data();
    const int* adj = g.adj.data();
    Stats s;
    size_t csz = 1;
    cur[0] = src;
    dist[src] = 0;
    while (csz) {
        fsz.push_back((long)csz);
        size_t nsz = 0;
        for (size_t i = 0; i < csz; ++i) {
            int u = cur[i];
            long b = off[u], e = off[u + 1];
            s.edges += e - b;
            int du = dist[u] + 1;
            for (long p = b; p < e; ++p) {
                int v = adj[p];
                if (dist[v] < 0) { dist[v] = du; nxt[nsz++] = v; }
            }
        }
        s.reached += (long)nsz;
        ++s.levels;
        std::swap(cur, nxt);
        csz = nsz;
    }
    return s;
}

static bool verify(const Graph& g, int src, const std::vector<int>& dist, const Stats& s) {
    if (dist[src] != 0) return false;
    long reached = 0, edges = 0;
    for (int u = 0; u < g.n; ++u) {
        if (dist[u] < 0) continue;
        ++reached;
        edges += g.offsets[u + 1] - g.offsets[u];
        if (u != src && dist[u] < 1) return false;
        bool has_parent = (u == src);
        for (long p = g.offsets[u]; p < g.offsets[u + 1]; ++p) {
            int v = g.adj[p];
            if (dist[v] < 0) return false;                       
            if (std::abs(dist[u] - dist[v]) > 1) return false;   
            if (dist[v] == dist[u] - 1) has_parent = true;       
        }
        if (!has_parent) return false;
    }
    return reached == s.reached && edges == s.edges;
}

 
int main(int argc, char** argv) {
    if (argc != 7) {
        std::cerr << "usage: " << argv[0] << " <graph_name> <file.mtx> <num_sources> <reps> <seed> <frontier_csv>\n";
        return 1;
    }
    const std::string name = argv[1];
    const char* path = argv[2];
    const int nsrc = std::stoi(argv[3]);
    const int reps = std::stoi(argv[4]);
    const uint64_t seed = std::stoull(argv[5]);
    const char* fpath = argv[6];
 
    Graph g;
    bool symmetric = false;
    if (!read_mtx(path, g, symmetric)) return 1;
    std::fprintf(stderr, "[%s] n=%d  adjacency entries m=%ld  undirected edges=%ld  avg degree=%.2f  (%s)\n",
                 name.c_str(), g.n, g.m, g.m / 2, (double)g.m / g.n, symmetric ? "symmetric" : "general");
 
    std::vector<int> cand;
    for (int v = 0; v < g.n; ++v) if (g.offsets[v + 1] > g.offsets[v]) cand.push_back(v);
    std::mt19937_64 rng(seed);
    std::shuffle(cand.begin(), cand.end(), rng);
    const int ns = std::min<int>(nsrc, (int)cand.size());
    if (ns < nsrc) std::fprintf(stderr, "warning: only %d vertices with nonzero degree\n", ns);
 
    std::vector<int> dist(g.n), cur(g.n), nxt(g.n);
    std::vector<long> fsz;
    fsz.reserve(4096);
 
    FILE* ff = std::fopen(fpath, "a");
    if (!ff) { std::fprintf(stderr, "cannot open %s\n", fpath); return 1; }
 
    std::vector<double> src_teps, src_frac;
    for (int si = 0; si < ns; ++si) {
        const int src = cand[si];
 
        std::fill(dist.begin(), dist.end(), -1);
        fsz.clear();
        Stats w = bfs(g, src, dist.data(), cur.data(), nxt.data(), fsz);
        if (!verify(g, src, dist, w)) { std::cerr << "verification failed for source " << src << "\n"; return 1; }
        for (size_t l = 0; l < fsz.size(); ++l)
            std::fprintf(ff, "%s,%d,%zu,%ld\n", name.c_str(), src, l, fsz[l]);
 
        std::vector<double> times;
        Stats s = w;
        for (int r = 0; r < reps; ++r) {
            std::fill(dist.begin(), dist.end(), -1);   
            fsz.clear();
            double t0 = now_ns();
            s = bfs(g, src, dist.data(), cur.data(), nxt.data(), fsz);
            double t1 = now_ns();
            g_sink = (double)s.reached;               
            times.push_back(t1 - t0);
            std::printf("%s,%d,%d,%.0f,%ld,%d,%ld\n", name.c_str(), src, r, t1 - t0, s.edges, s.levels, s.reached);
        }
        std::sort(times.begin(), times.end());
        double med = times[times.size() / 2];
        src_teps.push_back(s.edges / (med * 1e-9));
        src_frac.push_back((double)s.reached / g.n);
    }
    std::fclose(ff);
 
    std::vector<double> t = src_teps;
    std::sort(t.begin(), t.end());
    if (!t.empty()) {
        std::sort(src_frac.begin(), src_frac.end());
        std::fprintf(stderr, "[%s] TEPS over %d sources: min=%.4g  median=%.4g  max=%.4g   (reached fraction: min=%.4f max=%.4f)\n",
                     name.c_str(), ns, t.front(), t[t.size() / 2], t.back(), src_frac.front(), src_frac.back());
    }
    return 0;
}