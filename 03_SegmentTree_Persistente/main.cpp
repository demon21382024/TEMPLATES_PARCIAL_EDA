// Compilar:  g++ -std=c++17 -O2 main.cpp -o segtree && ./segtree
#include "segtree_persistente.hpp"
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

int main() {
    ejemplo();
    prueba_aleatoria();
}
