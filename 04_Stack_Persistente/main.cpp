// Compilar:  g++ -std=c++17 -O2 main.cpp -o stack && ./stack
#include "stack_persistente.h"
#include <iostream>
#include <random>
#include <cassert>

static void ejemplo() {
    PersistentStack<int> s;           // versión 0 = vacía
    int v1 = s.push(0, 1);            // [1]
    int v2 = s.push(v1, 2);           // [1,2]
    int v3 = s.push(v2, 3);           // [1,2,3]
    int v4 = s.pop(v3);               // [1,2]
    int v5 = s.push(v4, 9);           // [1,2,9]   (rama desde v4)
    std::cout << "top(v3)=" << s.top(v3) << " top(v5)=" << s.top(v5)
              << " size(v5)=" << s.size(v5) << " kth(v5,2)=" << s.kth(v5, 2)
              << " fondoComun(v3,v5)=" << s.commonBottom(v3, v5) << "\n";
    std::cout << "v5 (tope->fondo):";
    for (int x : s.toVector(v5)) std::cout << ' ' << x;
    std::cout << "\n";
}

static void prueba_aleatoria() {
    std::mt19937 rng(5);
    PersistentStack<int> s;
    std::vector<std::vector<int>> ref{{}};   // fondo -> tope
    std::vector<std::vector<int>> ids{{}};   // id único de cada push, para commonBottom
    int uid = 0;
    for (int it = 0; it < 20000; ++it) {
        int v = rng() % s.versions();
        if (rng() % 3 || ref[v].empty()) {
            int x = rng() % 1000;
            s.push(v, x);
            auto a = ref[v]; a.push_back(x); ref.push_back(a);
            auto b = ids[v]; b.push_back(++uid); ids.push_back(b);
        } else {
            s.pop(v);
            auto a = ref[v]; a.pop_back(); ref.push_back(a);
            auto b = ids[v]; b.pop_back(); ids.push_back(b);
        }
        int nv = s.versions() - 1;
        const auto& a = ref[nv];
        assert(s.size(nv) == (int)a.size());
        if (!a.empty()) {
            assert(s.top(nv) == a.back());
            int k = rng() % a.size();
            assert(s.kth(nv, k) == a[a.size() - 1 - k]);
        }
        int w = rng() % s.versions();
        size_t c = 0;
        while (c < ids[nv].size() && c < ids[w].size() && ids[nv][c] == ids[w][c]) ++c;
        assert(s.commonBottom(nv, w) == (int)c);
    }
    std::cout << "prueba aleatoria OK (" << s.versions() << " versiones, " << s.nodes() << " nodos)\n";
}

int main() {
    ejemplo();
    prueba_aleatoria();
}
