// Compilar:  g++ -std=c++17 -O2 main.cpp -o fibheap && ./fibheap
#include "fibonacci_heap_persistente.h"
#include "fibonacci_heap_parcial_fatnode.h"
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

// ---------------------------------------------------------------------------
//  Los 4 tipos de persistencia
// ---------------------------------------------------------------------------
static void demo_tipos() {
    std::cout << "\n== Tipos de persistencia ==\n";

    // 1) PARCIAL: versiones en línea; solo se modifica la última.
    {
        FH h(100, Persistencia::Parcial);
        FH::Handle a;
        int v1 = h.insert(0, 10, &a), v2 = h.insert(v1, 4), v3 = h.decreaseKey(v2, a, 1);
        std::cout << "[Parcial]    min: v1=" << h.getMin(v1) << " v2=" << h.getMin(v2) << " v3=" << h.getMin(v3);
        try { h.insert(v1, 7); } catch (const std::logic_error& e) { std::cout << "  | insert(v1) -> " << e.what(); }
        std::cout << "\n";

        PartialFibonacciHeap<int> f;                 // fat node
        FH::Handle b;
        f.insert(10, &b); f.insert(4); f.decreaseKey(b, 1);
        std::cout << "[Parcial/fat node] min: v1=" << f.getMin(1) << " v2=" << f.getMin(2)
                  << " v3=" << f.getMin(3) << "  clave de b: v2=" << f.keyOf(2, b) << " v3=" << f.keyOf(3, b) << "\n";
    }
    // 2) TOTAL: se modifica cualquier versión -> árbol de versiones.
    {
        FH h(100, Persistencia::Total);
        FH::Handle a;
        int v1 = h.insert(h.insert(0, 10, &a), 20);
        int ra = h.decreaseKey(v1, a, 2);            // rama A
        int rb = h.extractMin(v1);                   // rama B (a no está en B)
        std::cout << "[Total]      min(A)=" << h.getMin(ra) << " min(B)=" << h.getMin(rb)
                  << " a en B? " << h.contains(rb, a);
        try { h.merge(ra, rb); } catch (const std::logic_error& e) { std::cout << "  | merge -> " << e.what(); }
        std::cout << "\n";
    }
    // 3) CONFLUENTE: unir dos versiones (heaps disjuntos).
    {
        FH h(100, Persistencia::Confluente);
        int a = h.insert(h.insert(0, 7), 3);
        int b = h.insert(h.insert(0, 9), 5);
        int m = h.merge(a, b);
        std::cout << "[Confluente] merge: min=" << h.getMin(m) << " size=" << h.size(m);
        try { h.merge(m, a); } catch (const std::invalid_argument& e) { std::cout << "  | merge(m,a) -> " << e.what(); }
        std::cout << "\n";
    }
    // 4) FUNCIONAL: la memoria persistente nunca modifica un nodo publicado.
    {
        FH h(2000);
        int v = 0;
        for (int i = 0; i < 1000; ++i) v = h.insert(v, i);
        size_t antes = h.memoryBytes();
        int w = h.extractMin(v);
        std::cout << "[Funcional]  extractMin creo " << (h.memoryBytes() - antes) / 1024
                  << " KB nuevos sin tocar v: min(v)=" << h.getMin(v) << " min(w)=" << h.getMin(w) << "\n";
    }
}

static void prueba_fatnode() {
    // se modifica la última versión; se consulta cualquier versión
    std::mt19937 rng(31);
    const int OPS = 30000;
    PartialFibonacciHeap<int> f;
    std::vector<std::pair<int, std::map<int,int>>> snaps;   // (versión, handle -> clave)
    auto refMin = [](const std::map<int,int>& m) {
        int best = INT_MAX; for (auto& [id, k] : m) best = std::min(best, k); return best;
    };
    std::map<int,int> cur;
    for (int it = 0; it < OPS; ++it) {
        int op = rng() % 10;
        if (op < 4 || cur.empty()) {
            int k = rng() % 1000000; PartialFibonacciHeap<int>::Handle id;
            f.insert(k, &id); cur[id] = k;
        } else if (op < 6) {
            assert(f.getMin(f.latest()) == refMin(cur));
            int hm = f.minHandle(f.latest());
            f.extractMin(); cur.erase(hm);
        } else if (op < 9) {
            auto itr = std::next(cur.begin(), rng() % cur.size());
            int nk = itr->second - (int)(rng() % 5000);
            f.decreaseKey(itr->first, nk); itr->second = nk;
        } else {
            auto itr = std::next(cur.begin(), rng() % cur.size());
            f.erase(itr->first); cur.erase(itr);
        }
        int nv = f.latest();
        if (it % 10 == 0) snaps.push_back({nv, cur});             // guardamos algunas versiones
        assert(f.size(nv) == (int)cur.size());
        if (!cur.empty()) assert(f.getMin(nv) == refMin(cur));
    }
    // consultas sobre versiones viejas guardadas
    for (size_t i = 0; i < snaps.size(); i += 3) {
        int q = snaps[i].first;
        const auto& r = snaps[i].second;
        assert(f.size(q) == (int)r.size());
        if (!r.empty()) assert(f.getMin(q) == refMin(r));
        for (auto& [id, k] : r) { assert(f.contains(q, id) && f.keyOf(q, id) == k); }
        auto e = f.elements(q);
        std::vector<int> want;
        for (auto& [id, k] : r) want.push_back(k);
        std::sort(e.begin(), e.end()); std::sort(want.begin(), want.end());
        assert(e == want);
    }
    std::cout << "prueba parcial fat node OK (" << f.versions() << " versiones, "
              << f.memoryBytes() / 1024 << " KB)\n";
}

static void rendimiento() {
    // uso lineal típico: n inserciones + n extractMin, comparando las dos técnicas
    const int N = 100000;
    PersistentFibonacciHeap<int> h(N);
    PartialFibonacciHeap<int> f(N);
    std::mt19937 rng(1);
    int v = 0;
    for (int i = 0; i < N; ++i) { int x = rng(); v = h.insert(v, x); f.insert(x); }
    int prev = INT_MIN;
    for (int i = 0; i < N; ++i) {
        int x = h.getMin(v);
        assert(x >= prev && x == f.getMin(f.latest()));
        prev = x; v = h.extractMin(v); f.extractMin();
    }
    assert(f.getMin(N / 2) == h.getMin(N / 2));      // consulta a una versión vieja
    std::cout << "rendimiento: " << N << " insert + " << N << " extractMin OK\n"
              << "   memoria total/confluente (path copying): " << h.memoryBytes() / (1024 * 1024) << " MB\n"
              << "   memoria parcial (fat node):             " << f.memoryBytes() / (1024 * 1024) << " MB\n";
}

int main() {
    ejemplo();
    demo_tipos();
    std::cout << "\n";
    prueba_aleatoria();
    prueba_lineal_decrease();
    prueba_fatnode();
    rendimiento();
}
