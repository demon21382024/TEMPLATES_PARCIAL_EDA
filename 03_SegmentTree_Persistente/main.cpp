// Compilar:  g++ -std=c++17 -O2 main.cpp -o segtree && ./segtree
#include "segtree_persistente.h"
#include "segtree_parcial_fatnode.h"
#include <iostream>
#include <random>
#include <cassert>
#include <climits>

static void ejemplo() {
    // --- actualización puntual, suma ---
    PersistentSegTree<SumM<long long>> st(std::vector<long long>{1, 2, 3, 4, 5});
    int v1 = st.set(0, 2, 10);        // [1,2,10,4,5]
    int v2 = st.apply(v1, 0, 100);    // [101,2,10,4,5]
    std::cout << "suma[0..4]: v0=" << st.query(0, 0, 4) << " v1=" << st.query(v1, 0, 4)
              << " v2=" << st.query(v2, 0, 4) << "\n";

    // --- k-ésimo menor en subarreglo (árbol de conteo sobre valores comprimidos) ---
    std::vector<int> a = {5, 1, 4, 2, 3};
    std::vector<int> vals = a;
    std::sort(vals.begin(), vals.end());
    vals.erase(std::unique(vals.begin(), vals.end()), vals.end());
    PersistentSegTree<SumM<int>> cnt((int)vals.size());       // versión 0 = todo 0, sin memoria
    std::vector<int> ver = {0};
    for (int x : a) {
        int c = std::lower_bound(vals.begin(), vals.end(), x) - vals.begin();
        ver.push_back(cnt.apply(ver.back(), c, 1));
    }
    int l = 1, r = 3, k = 2;                                    // a[1..3] = {1,4,2}
    std::cout << "2do menor en a[1..3] = " << vals[cnt.kth(ver[l], ver[r + 1], k)] << "\n"; // 2

    // --- suma en rango con persistencia ---
    PersistentRangeAddSegTree<long long> ra(5);                 // [0,0,0,0,0]
    int w1 = ra.add(0, 1, 3, 5);                                // [0,5,5,5,0]
    int w2 = ra.add(w1, 0, 4, 1);                               // [1,6,6,6,1]
    std::cout << "suma[0..4]: w0=" << ra.query(0, 0, 4) << " w1=" << ra.query(w1, 0, 4)
              << " w2=" << ra.query(w2, 0, 4) << " w2[2]=" << ra.get(w2, 2) << "\n";
}

static void prueba_aleatoria() {
    std::mt19937 rng(3);
    const int N = 200;
    std::vector<long long> base(N);
    for (auto& x : base) x = rng() % 100;

    PersistentSegTree<SumM<long long>> s1(base);
    PersistentSegTree<MinM<long long>> s2(base);
    PersistentRangeAddSegTree<long long> s3(base);
    std::vector<std::vector<long long>> r1{base}, r3{base};
    for (int it = 0; it < 5000; ++it) {
        int v = rng() % s1.versions(), p = rng() % N;
        long long x = (long long)(rng() % 100) - 50;
        s1.set(v, p, x); s2.set(v, p, x);
        auto a = r1[v]; a[p] = x; r1.push_back(a);

        int w = rng() % s3.versions(), l = rng() % N, r = rng() % N;
        if (l > r) std::swap(l, r);
        s3.add(w, l, r, x);
        auto b = r3[w]; for (int i = l; i <= r; ++i) b[i] += x; r3.push_back(b);

        int q = rng() % s1.versions();
        l = rng() % N; r = rng() % N; if (l > r) std::swap(l, r);
        long long sum = 0, mn = LLONG_MAX, sum3 = 0;
        for (int i = l; i <= r; ++i) { sum += r1[q][i]; mn = std::min(mn, r1[q][i]); sum3 += r3[q][i]; }
        assert(s1.query(q, l, r) == sum);
        assert(s2.query(q, l, r) == mn);
        assert(s3.query(q, l, r) == sum3);
    }
    // kth contra ordenar
    std::vector<int> a(2000);
    for (auto& x : a) x = rng() % 50;
    PersistentSegTree<SumM<int>> cnt(50);
    std::vector<int> ver = {0};
    for (int x : a) ver.push_back(cnt.apply(ver.back(), x, 1));
    for (int q = 0; q < 2000; ++q) {
        int l = rng() % a.size(), r = rng() % a.size();
        if (l > r) std::swap(l, r);
        std::vector<int> sub(a.begin() + l, a.begin() + r + 1);
        std::sort(sub.begin(), sub.end());
        int k = rng() % sub.size() + 1;
        assert(cnt.kth(ver[l], ver[r + 1], k) == sub[k - 1]);
    }
    std::cout << "prueba aleatoria OK (nodos: " << s1.nodes() << ", " << s3.nodes()
              << ", conteo " << cnt.nodes() << ")\n";
}

// ---------------------------------------------------------------------------
//  Los 4 tipos de persistencia
// ---------------------------------------------------------------------------
static void demo_tipos() {
    std::cout << "\n== Tipos de persistencia ==\n";

    // 1) PARCIAL: versiones en línea; solo se modifica la última.
    {
        PersistentSegTree<SumM<int>> st(std::vector<int>{1, 2, 3}, Persistencia::Parcial);
        int v1 = st.set(0, 0, 10), v2 = st.set(v1, 2, 30);
        std::cout << "[Parcial]    suma: v0=" << st.query(0, 0, 2) << " v1=" << st.query(v1, 0, 2)
                  << " v2=" << st.query(v2, 0, 2);
        try { st.set(v1, 1, 5); } catch (const std::logic_error& e) { std::cout << "  | set(v1) -> " << e.what(); }
        std::cout << "\n";

        PartialSegTree<SumM<int>> f(std::vector<int>{1, 2, 3});     // fat node
        f.set(0, 10); f.set(2, 30);
        std::cout << "[Parcial/fat node] suma: v0=" << f.query(0, 0, 2) << " v1=" << f.query(1, 0, 2)
                  << " v2=" << f.query(2, 0, 2) << "\n";
    }
    // 2) TOTAL: se modifica cualquier versión -> árbol de versiones.
    {
        PersistentRangeAddSegTree<long long> ra(4, Persistencia::Total);
        int a = ra.add(0, 0, 1, 5);                                 // rama A: [5,5,0,0]
        int b = ra.add(0, 2, 3, 7);                                 // rama B: [0,0,7,7]
        std::cout << "[Total]      suma(A)=" << ra.query(a, 0, 3) << " suma(B)=" << ra.query(b, 0, 3);
        try { ra.merge(a, b); } catch (const std::logic_error& e) { std::cout << "  | merge -> " << e.what(); }
        std::cout << "\n";
    }
    // 3) CONFLUENTE: combinar dos versiones elemento a elemento.
    {
        PersistentRangeAddSegTree<long long> ra(4, Persistencia::Confluente);
        int a = ra.add(0, 0, 1, 5);                                 // [5,5,0,0]
        int b = ra.add(0, 1, 3, 7);                                 // [0,7,7,7]
        int m = ra.merge(a, b);                                     // [5,12,7,7]
        std::cout << "[Confluente] merge: a[1]=" << ra.get(m, 1) << " suma=" << ra.query(m, 0, 3);
        PersistentSegTree<MinM<int>> mn(std::vector<int>{4, 9, 2});
        int x = mn.set(0, 0, 1), y = mn.set(0, 2, 8);               // [1,9,2] y [4,9,8]
        int z = mn.merge(x, y);                                     // min elemento a elemento: [1,9,2]
        std::cout << " | min(merge)[0..2]=" << mn.query(z, 0, 2) << " a[2]=" << mn.get(z, 2) << "\n";
    }
    // 4) FUNCIONAL: nodos inmutables y compartidos.
    {
        PersistentSegTree<SumM<long long>> st(std::vector<long long>(1024, 1));
        size_t antes = st.nodes();
        int v = st.set(0, 500, 100);
        std::cout << "[Funcional]  set sobre n=1024 creo " << st.nodes() - antes << " nodos (log n + 1);"
                  << " suma v0=" << st.query(0, 0, 1023) << " suma v1=" << st.query(v, 0, 1023) << "\n";
    }
}

static void prueba_merge() {
    std::mt19937 rng(8);
    const int N = 64;
    PersistentSegTree<SumM<long long>> s(N);
    PersistentSegTree<MaxM<long long>> mx(std::vector<long long>(N, 0));
    PersistentRangeAddSegTree<long long> ra(N);
    std::vector<std::vector<long long>> r1{std::vector<long long>(N)}, r2 = r1, r3 = r1;
    for (int it = 0; it < 3000; ++it) {
        int v = rng() % s.versions(), w = rng() % s.versions();
        if (rng() % 4 == 0) {
            s.merge(v, w); mx.merge(v, w); ra.merge(v, w);
            auto a = r1[v], b = r2[v], c = r3[v];
            for (int i = 0; i < N; ++i) { a[i] += r1[w][i]; b[i] = std::max(b[i], r2[w][i]); c[i] += r3[w][i]; }
            r1.push_back(a); r2.push_back(b); r3.push_back(c);
        } else {
            int p = rng() % N, l = rng() % N, r = rng() % N;
            if (l > r) std::swap(l, r);
            long long x = (long long)(rng() % 100) - 50;
            s.apply(v, p, x); mx.set(v, p, x); ra.add(v, l, r, x);
            auto a = r1[v], b = r2[v], c = r3[v];
            a[p] += x; b[p] = x; for (int i = l; i <= r; ++i) c[i] += x;
            r1.push_back(a); r2.push_back(b); r3.push_back(c);
        }
        int q = s.versions() - 1, l = rng() % N, r = rng() % N;
        if (l > r) std::swap(l, r);
        long long s1 = 0, m2 = LLONG_MIN, s3 = 0;
        for (int i = l; i <= r; ++i) { s1 += r1[q][i]; m2 = std::max(m2, r2[q][i]); s3 += r3[q][i]; }
        assert(s.query(q, l, r) == s1 && mx.query(q, l, r) == m2 && ra.query(q, l, r) == s3);
    }
    std::cout << "prueba merge (confluente) OK\n";
}

static void prueba_fatnode() {
    // updates solo sobre la última versión; consultas en versiones al azar
    std::mt19937 rng(4);
    const int N = 100;
    std::vector<long long> base(N);
    for (auto& x : base) x = rng() % 100;
    PartialSegTree<SumM<long long>> f1(base);
    PartialSegTree<MinM<long long>> f2(base);
    PartialRangeAddSegTree<long long> f3(base);
    std::vector<std::vector<long long>> r1{base}, r3{base};
    for (int it = 0; it < 4000; ++it) {
        int p = rng() % N, l = rng() % N, r = rng() % N;
        if (l > r) std::swap(l, r);
        long long x = (long long)(rng() % 100) - 50;
        f1.set(p, x); f2.set(p, x); f3.add(l, r, x);
        auto a = r1.back(); a[p] = x; r1.push_back(a);
        auto b = r3.back(); for (int i = l; i <= r; ++i) b[i] += x; r3.push_back(b);

        int q = rng() % f1.versions();
        l = rng() % N; r = rng() % N; if (l > r) std::swap(l, r);
        long long s1 = 0, mn = LLONG_MAX, s3 = 0;
        for (int i = l; i <= r; ++i) { s1 += r1[q][i]; mn = std::min(mn, r1[q][i]); s3 += r3[q][i]; }
        assert(f1.query(q, l, r) == s1 && f2.query(q, l, r) == mn && f3.query(q, l, r) == s3);
        assert(f1.get(q, p) == r1[q][p]);
    }
    // kth en subarreglo con árbol de conteo parcial
    std::vector<int> a(2000);
    for (auto& x : a) x = rng() % 50;
    PartialSegTree<SumM<int>> cnt(50);
    for (int x : a) cnt.apply(x, 1);                 // versión i = prefijo a[0..i-1]
    for (int q = 0; q < 2000; ++q) {
        int l = rng() % a.size(), r = rng() % a.size();
        if (l > r) std::swap(l, r);
        std::vector<int> sub(a.begin() + l, a.begin() + r + 1);
        std::sort(sub.begin(), sub.end());
        int k = rng() % sub.size() + 1;
        assert(cnt.kth(l, r + 1, k) == sub[k - 1]);
    }
    std::cout << "prueba parcial fat node OK (entradas: " << f1.entries() << ", " << f3.entries()
              << ", conteo " << cnt.entries() << ")\n";
}

int main() {
    ejemplo();
    demo_tipos();
    std::cout << "\n";
    prueba_aleatoria();
    prueba_merge();
    prueba_fatnode();
}
