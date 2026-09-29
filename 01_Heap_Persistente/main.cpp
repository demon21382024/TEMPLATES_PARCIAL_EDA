// Compilar:  g++ -std=c++17 -O2 main.cpp -o heap && ./heap
#include "heap_persistente.h"
#include "heap_parcial_fatnode.h"
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

// ---------------------------------------------------------------------------
//  Los 4 tipos de persistencia
// ---------------------------------------------------------------------------
static void demo_tipos() {
    std::cout << "\n== Tipos de persistencia ==\n";

    // 1) PARCIAL: versiones en línea; solo se modifica la última.
    {
        PersistentHeap<int> h(Persistencia::Parcial);
        int v1 = h.push(0, 5), v2 = h.push(v1, 3), v3 = h.pop(v2);
        std::cout << "[Parcial]    top(v1)=" << h.top(v1) << " top(v2)=" << h.top(v2)
                  << " size(v3)=" << h.size(v3);
        try { h.push(v1, 7); } catch (const std::logic_error& e) { std::cout << "  | push(v1) -> " << e.what(); }
        std::cout << "\n";

        PartialPersistentHeap<int> f;          // misma idea con fat node
        f.push(5); f.push(3); f.pop();
        std::cout << "[Parcial/fat node] top(1)=" << f.top(1) << " top(2)=" << f.top(2)
                  << " size(3)=" << f.size(3) << "\n";
    }
    // 2) TOTAL: se modifica cualquier versión -> árbol de versiones.
    {
        PersistentHeap<int> h(Persistencia::Total);
        int v1 = h.push(0, 5);
        int a = h.push(v1, 3);                 // rama A desde v1
        int b = h.push(v1, 9);                 // rama B desde v1
        std::cout << "[Total]      top(A)=" << h.top(a) << " top(B)=" << h.top(b);
        try { h.merge(a, b); } catch (const std::logic_error& e) { std::cout << "  | merge -> " << e.what(); }
        std::cout << "\n";
    }
    // 3) CONFLUENTE: una versión nueva a partir de DOS versiones (DAG).
    {
        PersistentHeap<int> h(Persistencia::Confluente);
        int a = h.push(h.push(0, 4), 8);       // {4,8}
        int b = h.push(h.push(0, 1), 6);       // {1,6}
        int m = h.merge(a, b);                 // {1,4,6,8}
        int mm = h.merge(m, m);                // incluso consigo misma: {1,1,4,4,6,6,8,8}
        std::cout << "[Confluente] top(merge)=" << h.top(m) << " size(merge)=" << h.size(m)
                  << " size(merge(m,m))=" << h.size(mm) << "\n";
    }
    // 4) FUNCIONAL: ningún nodo publicado se modifica; las versiones comparten nodos.
    {
        PersistentHeap<int> h;
        int v = 0;
        for (int i = 1; i <= 1000; ++i) v = h.push(v, i);
        size_t antes = h.nodes();
        int w = h.push(v, 0);                  // solo copia la espina derecha
        std::cout << "[Funcional]  push sobre 1000 elementos creo " << h.nodes() - antes
                  << " nodos; v sigue intacta: top(v)=" << h.top(v) << " top(w)=" << h.top(w) << "\n";
    }
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
    std::cout << "\nprueba aleatoria (total/confluente) OK (" << h.versions() << " versiones, "
              << h.nodes() << " nodos)\n";
}

static void prueba_fatnode() {
    // operaciones sobre la última versión; consultas sobre versiones al azar
    std::mt19937 rng(99);
    PartialPersistentHeap<int> f;
    std::vector<std::multiset<int>> ref{{}};
    for (int it = 0; it < 3000; ++it) {
        auto s = ref.back();
        if (rng() % 3 || s.empty()) { int x = rng() % 1000; f.push(x); s.insert(x); }
        else { f.pop(); s.erase(s.begin()); }
        ref.push_back(std::move(s));
        int q = rng() % f.versions();
        assert(f.size(q) == (int)ref[q].size());
        if (!ref[q].empty()) {
            assert(f.top(q) == *ref[q].begin());
            auto k = f.topK(q, 5);
            auto itr = ref[q].begin();
            for (int x : k) assert(x == *itr++);
        }
    }
    std::cout << "prueba parcial fat node OK (" << f.versions() << " versiones, "
              << f.entries() << " entradas de historial)\n";
}

int main() {
    ejemplo();
    demo_tipos();
    prueba_aleatoria();
    prueba_fatnode();
}
