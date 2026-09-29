// Compilar:  g++ -std=c++17 -O2 main.cpp -o trie && ./trie
#include "trie_persistente.h"
#include "trie_parcial_fatnode.h"
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

// ---------------------------------------------------------------------------
//  Los 4 tipos de persistencia
// ---------------------------------------------------------------------------
static void demo_tipos() {
    std::cout << "\n== Tipos de persistencia ==\n";

    // 1) PARCIAL: versiones en línea; solo se modifica la última.
    {
        PersistentStringTrie<> st(Persistencia::Parcial);
        int v1 = st.insert(0, "sol"), v2 = st.insert(v1, "sal"), v3 = st.erase(v2, "sol");
        std::cout << "[Parcial]    prefijo 's': v1=" << st.countPrefix(v1, "s") << " v2="
                  << st.countPrefix(v2, "s") << " v3=" << st.countPrefix(v3, "s");
        try { st.insert(v1, "sur"); } catch (const std::logic_error& e) { std::cout << "  | insert(v1) -> " << e.what(); }
        std::cout << "\n";

        PartialStringTrie<> f;                      // misma idea con fat node
        f.insert("sol"); f.insert("sal"); f.erase("sol");
        std::cout << "[Parcial/fat node] prefijo 's': v1=" << f.countPrefix(1, "s") << " v2="
                  << f.countPrefix(2, "s") << " v3=" << f.countPrefix(3, "s") << "\n";
    }
    // 2) TOTAL: se modifica cualquier versión -> árbol de versiones.
    {
        PersistentXorTrie<8> tr(Persistencia::Total);
        int v1 = tr.insert(0, 5);
        int a = tr.insert(v1, 200);                 // rama A
        int b = tr.insert(v1, 3);                   // rama B
        std::cout << "[Total]      maxXor(A,0)=" << tr.maxXor(a, 0) << " maxXor(B,0)=" << tr.maxXor(b, 0);
        try { tr.merge(a, b); } catch (const std::logic_error& e) { std::cout << "  | merge -> " << e.what(); }
        std::cout << "\n";
    }
    // 3) CONFLUENTE: unión de dos versiones.
    {
        PersistentStringTrie<> st(Persistencia::Confluente);
        int a = st.insert(st.insert(0, "arbol"), "arena");
        int b = st.insert(st.insert(0, "arbol"), "barco");
        int m = st.merge(a, b);
        std::cout << "[Confluente] merge: size=" << st.size(m) << " count(arbol)=" << st.count(m, "arbol")
                  << " prefijo 'ar'=" << st.countPrefix(m, "ar") << "\n";
    }
    // 4) FUNCIONAL: nodos inmutables y compartidos.
    {
        PersistentXorTrie<20> tr;
        int v = 0;
        for (int i = 0; i < 1000; ++i) v = tr.insert(v, i * 7919 % (1 << 20));
        size_t antes = tr.nodes();
        int w = tr.insert(v, 12345);
        std::cout << "[Funcional]  insert creo " << tr.nodes() - antes << " nodos (camino de B+1);"
                  << " size(v)=" << tr.size(v) << " size(w)=" << tr.size(w) << "\n";
    }
}

static void prueba_merge() {
    std::mt19937 rng(11);
    PersistentXorTrie<10> tr;
    PersistentStringTrie<3, 'a'> st;
    std::vector<std::multiset<unsigned>> rx{{}};
    std::vector<std::multiset<std::string>> rs{{}};
    auto rnd = [&] { std::string s; int L = rng() % 4; while (L--) s += char('a' + rng() % 3); return s; };
    for (int it = 0; it < 3000; ++it) {
        int v = rng() % tr.versions(), w = rng() % tr.versions();
        if (rng() % 4 == 0 && rx[v].size() + rx[w].size() < 300) {
            tr.merge(v, w); st.merge(v, w);
            auto a = rx[v]; a.insert(rx[w].begin(), rx[w].end()); rx.push_back(a);
            auto b = rs[v]; b.insert(rs[w].begin(), rs[w].end()); rs.push_back(b);
        } else {
            unsigned x = rng() % 1024; std::string s = rnd();
            tr.insert(v, x); st.insert(v, s);
            auto a = rx[v]; a.insert(x); rx.push_back(a);
            auto b = rs[v]; b.insert(s); rs.push_back(b);
        }
        int nv = tr.versions() - 1;
        assert(tr.size(nv) == (int)rx[nv].size() && st.size(nv) == (int)rs[nv].size());
        if (!rx[nv].empty()) {
            int k = rng() % rx[nv].size() + 1;
            assert(tr.kth(nv, k) == *std::next(rx[nv].begin(), k - 1));
            assert(st.kth(nv, k % rs[nv].size() + 1) == *std::next(rs[nv].begin(), k % rs[nv].size()));
        }
    }
    std::cout << "prueba merge (confluente) OK\n";
}

static void prueba_fatnode() {
    // se modifica solo la última versión; se consultan versiones al azar,
    // comparando contra la versión path copying en modo Parcial
    std::mt19937 rng(21);
    PartialXorTrie<10> fx;       PersistentXorTrie<10> px(Persistencia::Parcial);
    PartialStringTrie<3, 'a'> fs; PersistentStringTrie<3, 'a'> ps(Persistencia::Parcial);
    std::vector<std::multiset<std::string>> rs{{}};
    auto rnd = [&] { std::string s; int L = rng() % 5; while (L--) s += char('a' + rng() % 3); return s; };
    for (int it = 0; it < 4000; ++it) {
        unsigned x = rng() % 1024;
        if (rng() % 3 || px.count(px.latest(), x) == 0) { fx.insert(x); px.insert(px.latest(), x); }
        else { fx.erase(x); px.erase(px.latest(), x); }
        std::string s = rnd();
        auto cur = rs.back();
        if (rng() % 3 || !cur.count(s)) { fs.insert(s); ps.insert(ps.latest(), s); cur.insert(s); }
        else { fs.erase(s); ps.erase(ps.latest(), s); cur.erase(cur.find(s)); }
        rs.push_back(cur);

        int vr = rng() % fx.versions();
        unsigned q = rng() % 1024;
        assert(fx.size(vr) == px.size(vr) && fx.count(vr, q) == px.count(vr, q));
        assert(fx.countLess(vr, q) == px.countLess(vr, q));
        if (px.size(vr) > 0) {
            assert(fx.maxXor(vr, q) == px.maxXor(vr, q) && fx.minXor(vr, q) == px.minXor(vr, q));
            int k = rng() % px.size(vr) + 1;
            assert(fx.kth(vr, k) == px.kth(vr, k));
        }
        std::string p = rnd();
        assert(fs.countPrefix(vr, p) == ps.countPrefix(vr, p) && fs.count(vr, p) == ps.count(vr, p));
        if (!rs[vr].empty()) {
            int k = rng() % rs[vr].size() + 1;
            assert(fs.kth(vr, k) == *std::next(rs[vr].begin(), k - 1));
        }
    }
    // consultas sobre subarreglos (vr - vl): versión i = prefijo a[0..i-1]
    PartialXorTrie<10> f2; PersistentXorTrie<10> p2(Persistencia::Parcial);
    for (int i = 0; i < 2000; ++i) { unsigned x = rng() % 1024; f2.insert(x); p2.insert(p2.latest(), x); }
    for (int it = 0; it < 2000; ++it) {
        int vl = rng() % 2000, vr = vl + 1 + rng() % (2000 - vl);
        unsigned q = rng() % 1024;
        assert(f2.countLess(vl, vr, q) == p2.countLess(vl, vr, q));
        assert(f2.maxXor(vl, vr, q) == p2.maxXor(vl, vr, q) && f2.minXor(vl, vr, q) == p2.minXor(vl, vr, q));
        int k = rng() % (vr - vl) + 1;
        assert(f2.kth(vl, vr, k) == p2.kth(vl, vr, k));
    }
    std::cout << "prueba parcial fat node OK (xor: " << fx.entries() << " entradas vs "
              << px.nodes() << " nodos path copying; cadenas: " << fs.entries() << " entradas vs "
              << ps.nodes() << " nodos)\n";
}

int main() {
    ejemplo();
    demo_tipos();
    std::cout << "\n";
    prueba_xor();
    prueba_strings();
    prueba_merge();
    prueba_fatnode();
}
