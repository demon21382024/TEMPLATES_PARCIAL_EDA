// Compilar:  g++ -std=c++17 -O2 main.cpp -o trie && ./trie
#include "trie_persistente.h"
#include <iostream>
#include <random>
#include <set>
#include <algorithm>
#include <cassert>

static void ejemplo() {
    // --- XOR sobre subarreglos: versión i = prefijo a[0..i-1] ---
    std::vector<unsigned> a = {3, 10, 5, 25, 2, 8};
    PersistentXorTrie<30> tr;
    std::vector<int> ver = {0};
    for (unsigned x : a) ver.push_back(tr.insert(ver.back(), x));
    int l = 1, r = 3;                                   // subarreglo a[1..3] = {10,5,25}
    std::cout << "max xor con 7 en a[1..3] = " << tr.maxXor(ver[l], ver[r + 1], 7) << "\n";  // 25^7=30
    std::cout << "2do menor en a[1..3]    = " << tr.kth(ver[l], ver[r + 1], 2) << "\n";     // 10
    std::cout << "# < 11 en todo a        = " << tr.countLess(ver.back(), 11) << "\n";      // 5

    // --- Trie de palabras ---
    PersistentStringTrie<> st;
    int v1 = st.insert(0, "casa");
    int v2 = st.insert(v1, "carro");
    int v3 = st.insert(v2, "cama");
    int v4 = st.erase(v3, "casa");
    std::cout << "v3: prefijo 'ca'=" << st.countPrefix(v3, "ca")
              << "  v4: prefijo 'ca'=" << st.countPrefix(v4, "ca")
              << "  v4: count(casa)=" << st.count(v4, "casa")
              << "  v1: count(casa)=" << st.count(v1, "casa") << "\n";
    std::cout << "v3 en orden: " << st.kth(v3, 1) << ' ' << st.kth(v3, 2) << ' ' << st.kth(v3, 3) << "\n";
}

static void prueba_xor() {
    std::mt19937 rng(7);
    const int N = 3000;
    std::vector<unsigned> a(N);
    for (auto& x : a) x = rng() % (1u << 12);
    PersistentXorTrie<12> tr;
    std::vector<int> ver = {0};
    for (unsigned x : a) ver.push_back(tr.insert(ver.back(), x));
    for (int q = 0; q < 3000; ++q) {
        int l = rng() % N, r = rng() % N;
        if (l > r) std::swap(l, r);
        unsigned x = rng() % (1u << 12);
        std::vector<unsigned> sub(a.begin() + l, a.begin() + r + 1);
        unsigned mx = 0, mn = ~0u;
        for (unsigned y : sub) { mx = std::max(mx, x ^ y); mn = std::min(mn, x ^ y); }
        assert(tr.maxXor(ver[l], ver[r + 1], x) == mx);
        assert(tr.minXor(ver[l], ver[r + 1], x) == mn);
        std::sort(sub.begin(), sub.end());
        int k = rng() % sub.size() + 1;
        assert(tr.kth(ver[l], ver[r + 1], k) == sub[k - 1]);
        int less = std::lower_bound(sub.begin(), sub.end(), x) - sub.begin();
        assert(tr.countLess(ver[l], ver[r + 1], x) == less);
    }
    // inserciones / borrados sobre versiones arbitrarias
    PersistentXorTrie<10> t2;
    std::vector<std::multiset<unsigned>> ref{{}};
    for (int it = 0; it < 3000; ++it) {
        int v = rng() % t2.versions();
        unsigned x = rng() % 64;
        if (rng() % 2 || ref[v].empty()) { t2.insert(v, x); auto s = ref[v]; s.insert(x); ref.push_back(s); }
        else { x = *ref[v].begin(); t2.erase(v, x); auto s = ref[v]; s.erase(s.find(x)); ref.push_back(s); }
        int nv = t2.versions() - 1;
        assert(t2.size(nv) == (int)ref[nv].size());
        assert(t2.count(nv, x) == (int)ref[nv].count(x));
    }
    std::cout << "prueba XOR trie OK (" << tr.nodes() + t2.nodes() << " nodos)\n";
}

static void prueba_strings() {
    std::mt19937 rng(9);
    PersistentStringTrie<3, 'a'> st;            // alfabeto {a,b,c}
    std::vector<std::multiset<std::string>> ref{{}};
    auto rnd = [&] { std::string s; int L = rng() % 5; while (L--) s += char('a' + rng() % 3); return s; };
    for (int it = 0; it < 4000; ++it) {
        int v = rng() % st.versions();
        std::string s = rnd();
        if (rng() % 3 || !ref[v].count(s)) { st.insert(v, s); auto r = ref[v]; r.insert(s); ref.push_back(r); }
        else { st.erase(v, s); auto r = ref[v]; r.erase(r.find(s)); ref.push_back(r); }
        int nv = st.versions() - 1;
        std::string p = rnd(), w = rnd();
        int pc = 0;
        for (auto& x : ref[nv]) pc += x.compare(0, p.size(), p) == 0;
        assert(st.countPrefix(nv, p) == pc);
        assert(st.count(nv, w) == (int)ref[nv].count(w));
        if (!ref[nv].empty()) {
            int k = rng() % ref[nv].size() + 1;
            assert(st.kth(nv, k) == *std::next(ref[nv].begin(), k - 1));
        }
    }
    std::cout << "prueba string trie OK (" << st.nodes() << " nodos)\n";
}

int main() {
    ejemplo();
    prueba_xor();
    prueba_strings();
}
