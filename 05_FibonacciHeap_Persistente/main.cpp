// Compilar:  g++ -std=c++17 -O2 main.cpp -o fibheap && ./fibheap
#include "fibonacci_heap_persistente.hpp"
#include <iostream>
#include <random>
#include <map>
#include <set>
#include <algorithm>
#include <cassert>
#include <climits>

using FH = PersistentFibonacciHeap<int>;

static void ejemplo() {
    FH h(1000);                         // hasta 1000 inserciones en total
    FH::Handle a, b, c;
    int v1 = h.insert(0, 10, &a);       // {10}
    int v2 = h.insert(v1, 20, &b);      // {10,20}
    int v3 = h.insert(v2, 30, &c);      // {10,20,30}
    int v4 = h.extractMin(v3);          // {20,30}
    int v5 = h.decreaseKey(v4, c, 5);   // {5,20}
    int v6 = h.decreaseKey(v3, c, 1);   // {1,10,20}   <- el handle sirve en otra rama
    int v7 = h.erase(v6, b);            // {1,10}

    std::cout << "min: v3=" << h.getMin(v3) << " v4=" << h.getMin(v4) << " v5=" << h.getMin(v5)
              << " v6=" << h.getMin(v6) << " v7=" << h.getMin(v7) << "  size(v7)=" << h.size(v7) << "\n";
    std::cout << "clave de c en v4=" << h.keyOf(v4, c) << ", en v5=" << h.keyOf(v5, c)
              << "; a esta en v4? " << h.contains(v4, a) << "\n";

    // merge de heaps disjuntos
    int x = 0, y = 0;
    for (int k : {7, 3, 9}) x = h.insert(x, k);
    for (int k : {4, 8})    y = h.insert(y, k);
    int m = h.merge(x, y);
    std::cout << "merge: min=" << h.getMin(m) << " size=" << h.size(m) << "\n";

    // Dijkstra usando la última versión (uso lineal)
    std::vector<std::vector<std::pair<int,int>>> g(5);
    auto edge = [&](int u, int w, int d) { g[u].push_back({w, d}); g[w].push_back({u, d}); };
    edge(0, 1, 4); edge(0, 2, 1); edge(2, 1, 2); edge(1, 3, 1); edge(2, 3, 5); edge(3, 4, 3);
    PersistentFibonacciHeap<std::pair<int,int>> pq(100);   // (dist, nodo)
    std::vector<int> dist(5, 1e9), hd(5, 0);
    int ver = 0;
    dist[0] = 0;
    for (int u = 0; u < 5; ++u) ver = pq.insert(ver, {dist[u], u}, &hd[u]);
    while (!pq.empty(ver)) {
        auto [du, u] = pq.getMin(ver);
        ver = pq.extractMin(ver);
        for (auto [w, d] : g[u])
            if (du + d < dist[w]) { dist[w] = du + d; ver = pq.decreaseKey(ver, hd[w], {dist[w], w}); }
    }
    std::cout << "dijkstra:";
    for (int d : dist) std::cout << ' ' << d;
    std::cout << "\n";
}

static void prueba_aleatoria() {
    std::mt19937 rng(2024);
    const int OPS = 20000;
    FH h(OPS + 10);
    std::vector<std::map<int,int>> ref{{}};       // handle -> clave, por versión
    auto refMin = [](const std::map<int,int>& m) {
        int best = INT_MAX; for (auto& [id, k] : m) best = std::min(best, k); return best;
    };
    for (int it = 0; it < OPS; ++it) {
        int v = rng() % h.versions();
        int op = rng() % 10;
        auto s = ref[v];
        if (op < 4 || s.empty()) {
            int k = rng() % 100000; FH::Handle id;
            h.insert(v, k, &id);
            s[id] = k;
        } else if (op < 6) {
            h.extractMin(v);
            int best = refMin(s);
            // el heap puede extraer cualquiera con la clave mínima: lo buscamos
            // en la nueva versión para saber cuál fue
            int nv = h.versions() - 1;
            for (auto itr = s.begin(); itr != s.end(); ++itr)
                if (itr->second == best && !h.contains(nv, itr->first)) { s.erase(itr); break; }
        } else if (op < 9) {
            auto itr = std::next(s.begin(), rng() % s.size());
            int nk = itr->second - (int)(rng() % 1000);
            h.decreaseKey(v, itr->first, nk);
            itr->second = nk;
        } else {
            auto itr = std::next(s.begin(), rng() % s.size());
            h.erase(v, itr->first);
            s.erase(itr);
        }
        ref.push_back(std::move(s));
        int nv = h.versions() - 1;
        const auto& r = ref[nv];
        assert(h.size(nv) == (int)r.size());
        if (!r.empty()) assert(h.getMin(nv) == refMin(r));
        if (it % 500 == 0) {           // verificación completa de una versión al azar
            int q = rng() % h.versions();
            auto e = h.elements(q);
            std::vector<int> want;
            for (auto& [id, k] : ref[q]) { want.push_back(k); assert(h.keyOf(q, id) == k); }
            std::sort(e.begin(), e.end()); std::sort(want.begin(), want.end());
            assert(e == want);
        }
    }
    // merge de heaps disjuntos contra referencia
    FH g(4000);
    int a = 0, b = 0; std::multiset<int> ra, rb;
    for (int i = 0; i < 1000; ++i) { int k = rng() % 5000; a = g.insert(a, k); ra.insert(k); }
    for (int i = 0; i < 20; ++i) { a = g.extractMin(a); ra.erase(ra.begin()); }
    for (int i = 0; i < 1000; ++i) { int k = rng() % 5000; b = g.insert(b, k); rb.insert(k); }
    int m = g.merge(a, b); ra.insert(rb.begin(), rb.end());
    while (!ra.empty()) { assert(g.getMin(m) == *ra.begin()); m = g.extractMin(m); ra.erase(ra.begin()); }
    bool threw = false;
    try { g.merge(a, a); } catch (const std::invalid_argument&) { threw = true; }
    assert(threw);
    std::cout << "prueba aleatoria OK (" << h.versions() << " versiones, "
              << h.memoryBytes() / 1024 << " KB)\n";
}

static void prueba_lineal_decrease() {
    // uso lineal (siempre la última versión) con muchos decreaseKey:
    // genera árboles profundos y cascading cuts
    std::mt19937 rng(77);
    const int OPS = 60000;
    FH h(OPS);
    std::set<std::pair<int,int>> ref;           // (clave, handle)
    std::map<int,int> keyOf;                    // handle -> clave
    int v = 0;
    for (int it = 0; it < OPS; ++it) {
        int op = rng() % 10;
        if (op < 4 || ref.empty()) {
            int k = rng() % 1000000; FH::Handle id;
            v = h.insert(v, k, &id);
            ref.insert({k, id}); keyOf[id] = k;
        } else if (op < 5) {
            int k = h.getMin(v);
            assert(k == ref.begin()->first);
            int hm = h.minHandle(v);
            v = h.extractMin(v);
            ref.erase({k, hm}); keyOf.erase(hm);
        } else {
            auto itr = keyOf.lower_bound(rng() % (it + 1) + 1);
            if (itr == keyOf.end()) continue;
            int id = itr->first, nk = itr->second - (int)(rng() % 5000);
            v = h.decreaseKey(v, id, nk);
            ref.erase({itr->second, id}); ref.insert({nk, id}); itr->second = nk;
        }
        assert(h.size(v) == (int)ref.size());
        if (!ref.empty()) assert(h.getMin(v) == ref.begin()->first);
    }
    std::cout << "prueba lineal con decreaseKey OK (memoria " << h.memoryBytes() / (1024 * 1024) << " MB)\n";
}

static void rendimiento() {
    // uso lineal típico: n inserciones + n extractMin
    const int N = 100000;
    PersistentFibonacciHeap<int> h(N);
    std::mt19937 rng(1);
    int v = 0;
    for (int i = 0; i < N; ++i) v = h.insert(v, rng());
    int prev = INT_MIN;
    for (int i = 0; i < N; ++i) { int x = h.getMin(v); assert(x >= prev); prev = x; v = h.extractMin(v); }
    std::cout << "rendimiento: " << N << " insert + " << N << " extractMin OK, memoria="
              << h.memoryBytes() / (1024 * 1024) << " MB\n";
}

int main() {
    ejemplo();
    prueba_aleatoria();
    prueba_lineal_decrease();
    rendimiento();
}
