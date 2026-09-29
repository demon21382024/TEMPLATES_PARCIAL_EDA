// Compilar:  g++ -std=c++17 -O2 main.cpp -o stack && ./stack
#include "stack_persistente.h"
#include "stack_parcial_fatnode.h"
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

// ---------------------------------------------------------------------------
//  Los 4 tipos de persistencia
// ---------------------------------------------------------------------------
static void imprimir(const char* nombre, const std::vector<int>& v) {
    std::cout << nombre << "=[";
    for (size_t i = 0; i < v.size(); ++i) std::cout << (i ? "," : "") << v[i];
    std::cout << "]";
}

static void demo_tipos() {
    std::cout << "\n== Tipos de persistencia (pilas del tope al fondo) ==\n";

    // 1) PARCIAL: versiones en línea; solo se modifica la última.
    {
        PersistentStack<int> s(Persistencia::Parcial);
        int v1 = s.push(0, 1), v2 = s.push(v1, 2), v3 = s.pop(v2);
        std::cout << "[Parcial]    "; imprimir("v2", s.toVector(v2)); std::cout << " "; imprimir("v3", s.toVector(v3));
        try { s.push(v1, 9); } catch (const std::logic_error& e) { std::cout << "  | push(v1) -> " << e.what(); }
        std::cout << "\n";

        PartialStack<int> f;                         // fat node
        f.push(1); f.push(2); f.pop();
        std::cout << "[Parcial/fat node] "; imprimir("v2", f.toVector(2)); std::cout << " ";
        imprimir("v3", f.toVector(3)); std::cout << "\n";
    }
    // 2) TOTAL: se modifica cualquier versión -> árbol de versiones.
    {
        PersistentStack<int> s(Persistencia::Total);
        int v1 = s.push(0, 1);
        int a = s.push(v1, 2), b = s.push(v1, 3);   // dos ramas desde v1
        std::cout << "[Total]      "; imprimir("A", s.toVector(a)); std::cout << " "; imprimir("B", s.toVector(b));
        std::cout << " fondoComun=" << s.commonBottom(a, b);
        try { s.concat(a, b); } catch (const std::logic_error& e) { std::cout << "  | concat -> " << e.what(); }
        std::cout << "\n";
    }
    // 3) CONFLUENTE: una pila nueva a partir de DOS versiones.
    {
        PersistentStack<int> s(Persistencia::Confluente);
        int a = s.push(s.push(0, 1), 2);             // [2,1]
        int b = s.push(s.push(0, 8), 9);             // [9,8]
        int c = s.concat(a, b);                      // b encima de a: [9,8,2,1]
        std::cout << "[Confluente] "; imprimir("concat(a,b)", s.toVector(c));
        std::cout << " fondoComun(a,concat)=" << s.commonBottom(a, c) << "\n";
    }
    // 4) FUNCIONAL: nodos inmutables y compartidos.
    {
        PersistentStack<int> s;
        int v = 0;
        for (int i = 0; i < 1000; ++i) v = s.push(v, i);
        size_t antes = s.nodes();
        int w = s.pop(s.pop(v));
        std::cout << "[Funcional]  2 pops crearon " << s.nodes() - antes << " nodos; top(v)=" << s.top(v)
                  << " top(w)=" << s.top(w) << "\n";
    }
}

static void prueba_concat() {
    std::mt19937 rng(6);
    PersistentStack<int> s;
    std::vector<std::vector<int>> ref{{}};           // fondo -> tope
    for (int it = 0; it < 5000; ++it) {
        int v = rng() % s.versions();
        int op = rng() % 5;
        if (op == 0) {
            int w = rng() % s.versions();
            if (ref[v].size() + ref[w].size() > 3000) continue;
            s.concat(v, w);
            auto a = ref[v]; a.insert(a.end(), ref[w].begin(), ref[w].end()); ref.push_back(a);
        } else if (op == 1 && !ref[v].empty()) {
            s.pop(v); auto a = ref[v]; a.pop_back(); ref.push_back(a);
        } else {
            int x = rng() % 1000; s.push(v, x); auto a = ref[v]; a.push_back(x); ref.push_back(a);
        }
        int nv = s.versions() - 1;
        const auto& a = ref[nv];
        assert(s.size(nv) == (int)a.size());
        if (!a.empty()) {
            int k = rng() % a.size();
            assert(s.kth(nv, k) == a[a.size() - 1 - k]);
        }
    }
    std::cout << "prueba concat (confluente) OK\n";
}

static void prueba_fatnode() {
    std::mt19937 rng(15);
    PartialStack<int> f;
    std::vector<std::vector<int>> ref{{}};
    for (int it = 0; it < 20000; ++it) {
        auto a = ref.back();
        if (rng() % 3 || a.empty()) { int x = rng() % 1000; f.push(x); a.push_back(x); }
        else { f.pop(); a.pop_back(); }
        ref.push_back(std::move(a));
        int q = rng() % f.versions();
        const auto& r = ref[q];
        assert(f.size(q) == (int)r.size());
        if (!r.empty()) {
            assert(f.top(q) == r.back());
            int k = rng() % r.size();
            assert(f.kth(q, k) == r[r.size() - 1 - k]);
        }
    }
    std::cout << "prueba parcial fat node OK (" << f.versions() << " versiones, "
              << f.entries() << " entradas de historial)\n";
}

int main() {
    ejemplo();
    demo_tipos();
    std::cout << "\n";
    prueba_aleatoria();
    prueba_concat();
    prueba_fatnode();
}
