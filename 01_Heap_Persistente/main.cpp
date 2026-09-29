// Compilar:  g++ -std=c++17 -O2 main.cpp -o heap && ./heap
#include "heap_persistente.hpp"
#include <iostream>
#include <set>
#include <random>
#include <cassert>

static void ejemplo() {
    PersistentHeap<int> h;             // min-heap; versión 0 = vacío
    int v1 = h.push(0, 5);             // {5}
    int v2 = h.push(v1, 3);            // {3,5}
    int v3 = h.push(v2, 8);            // {3,5,8}
    int v4 = h.pop(v3);                // {5,8}
    int v5 = h.push(v2, 1);            // {1,3,5}   <- rama desde v2
    int v6 = h.merge(v4, v5);          // {1,3,5,5,8}

    std::cout << "top(v3)=" << h.top(v3) << " top(v4)=" << h.top(v4)
              << " top(v5)=" << h.top(v5) << " top(v6)=" << h.top(v6)
              << " size(v6)=" << h.size(v6) << "\n";
    std::cout << "topK(v6,5):";
    for (int x : h.topK(v6, 5)) std::cout << ' ' << x;
    std::cout << "\n";

    PersistentHeap<int, std::greater<int>> mx;  // max-heap
    int b = mx.build({4, 9, 1, 7});
    std::cout << "max-heap build top=" << mx.top(b) << "\n";
}

static void prueba_aleatoria() {
    std::mt19937 rng(12345);
    PersistentHeap<int> h;
    std::vector<std::multiset<int>> ref{{}};
    for (int it = 0; it < 200000; ++it) {
        int v = rng() % h.versions();
        int op = rng() % 10;
        if (op < 5) {
            int x = rng() % 1000;
            h.push(v, x);
            auto s = ref[v]; s.insert(x); ref.push_back(std::move(s));
        } else if (op < 8 && !ref[v].empty()) {
            h.pop(v);
            auto s = ref[v]; s.erase(s.begin()); ref.push_back(std::move(s));
        } else if (op == 8) {
            int w = rng() % h.versions();
            if (ref[v].size() + ref[w].size() > 2000) continue;   // acota la fuerza bruta
            h.merge(v, w);
            auto s = ref[v]; s.insert(ref[w].begin(), ref[w].end()); ref.push_back(std::move(s));
        } else continue;
        int nv = h.versions() - 1;
        assert(h.size(nv) == (int)ref[nv].size());
        if (!ref[nv].empty()) assert(h.top(nv) == *ref[nv].begin());
        if (ref.size() > 3000) break;   // la referencia copia sets: limitamos
    }
    std::cout << "prueba aleatoria OK (" << h.versions() << " versiones, "
              << h.nodes() << " nodos)\n";
}

int main() {
    ejemplo();
    prueba_aleatoria();
}
