#pragma once
// ============================================================================
//  TRIE PARCIALMENTE PERSISTENTE  -  trie normal + FAT NODE
// ----------------------------------------------------------------------------
//  Persistencia PARCIAL: se CONSULTA cualquier versión, solo se MODIFICA la
//  última (versiones en línea 0,1,2,...).
//
//  Técnica fat node (Driscoll et al.): el trie es UNO solo (efímero). Los
//  punteros a hijos se escriben una única vez (0 -> nodo nuevo) y nunca
//  cambian, así que no necesitan historial. Solo los CONTADORES cambian y cada
//  uno guarda su historial [(versión, valor)]. Un nodo creado después de la
//  versión v tiene contador 0 en v => en v "no existe".
//
//  Comparado con path copying: misma memoria asintótica O(L) por inserción,
//  pero cada entrada es más chica (versión + contador) y no se duplican los
//  arreglos de hijos (clave en el trie de cadenas: SIGMA ints por nodo).
//  A cambio, cada lectura en versión vieja cuesta O(log m) (búsqueda binaria).
//
//  Complejidades (m = # de modificaciones de un contador):
//    insert / erase (última versión) : O(L)
//    consultas en versión v          : O(L · log m)
//  NO es funcional: los nodos se modifican agregando historial.
// ============================================================================
#include <vector>
#include <array>
#include <string>
#include <cstdint>
#include <stdexcept>
#include <algorithm>

#ifndef EDA_FAT_ARRAY_H
#define EDA_FAT_ARRAY_H
// ---------------------------------------------------------------------------
//  FatArray<T>: arreglo PARCIALMENTE persistente (fat node).
//    get(i)       valor actual                          O(1)
//    get(v, i)    valor en la versión v (búsq. binaria)  O(log m)
//    mut(i)/set   modifica en la versión actual: si la última entrada ya es de
//                 esta versión se pisa, si no se agrega una   O(1) amortizado
//    newVersion() congela la versión actual y abre la siguiente
//    add(x)       celda nueva que existe desde la versión actual
//  Una celda leída en una versión anterior a su creación devuelve T().
// ---------------------------------------------------------------------------
template <class T>
class FatArray {
    struct Entry { int ver; T val; };
    std::vector<std::vector<Entry>> h;      // historial de cada celda
    int now = 0;
    size_t total = 0;                       // # total de entradas (memoria)
public:
    int  version() const { return now; }
    int  newVersion() { return ++now; }
    int  add(const T& init = T()) {
        h.push_back(std::vector<Entry>{Entry{now, init}});
        ++total;
        return (int)h.size() - 1;
    }
    int  size() const { return (int)h.size(); }
    const T& get(int i) const { return h[i].back().val; }
    T get(int v, int i) const {
        const auto& c = h[i];
        if (c.back().ver <= v) return c.back().val;          // caso rápido
        auto it = std::upper_bound(c.begin(), c.end(), v,
                                   [](int x, const Entry& e) { return x < e.ver; });
        return it == c.begin() ? T() : std::prev(it)->val;
    }
    T& mut(int i) {
        auto& c = h[i];
        if (c.back().ver != now) { T copy = c.back().val; c.push_back(Entry{now, std::move(copy)}); ++total; }
        return c.back().val;
    }
    void set(int i, const T& x) { mut(i) = x; }
    void reserve(size_t n) { h.reserve(n); }
    size_t entries() const { return total; }
};
#endif

// ---------------------------------------------------------------------------
// 1) Trie binario parcialmente persistente
// ---------------------------------------------------------------------------
template <int B = 30>
class PartialXorTrie {
    static_assert(B >= 1 && B <= 63, "B entre 1 y 63");
public:
    using U = uint64_t;
private:
    std::vector<std::array<int, 2>> ch;   // hijos (0 = no hay); se escriben una vez
    FatArray<int> cnt;                    // contador con historial
    static constexpr int ROOT = 1;        // nodo 0 = nulo (contador siempre 0)

    void check(int v) const {
        if (v < 0 || v > cnt.version()) throw std::out_of_range("version invalida");
    }
    int C(int v, int x) const { return x ? cnt.get(v, x) : 0; }
    int c(int vl, int vr, int x) const { return C(vr, x) - C(vl, x); }   // "vr - vl"
    int child(int x, int bit) {
        if (!ch[x][bit]) {
            int y = (int)ch.size();
            ch.push_back({0, 0});
            cnt.add(0);
            ch[x][bit] = y;
        }
        return ch[x][bit];
    }
    int addPath(U x, int d) {
        cnt.newVersion();
        int a = ROOT;
        cnt.mut(a) += d;
        for (int b = B - 1; b >= 0; --b) {
            a = child(a, (x >> b) & 1);
            cnt.mut(a) += d;
        }
        return latest();
    }

public:
    explicit PartialXorTrie(size_t reserveNodes = 0) {
        ch.reserve(reserveNodes + 2);
        cnt.reserve(reserveNodes + 2);
        ch.push_back({0, 0}); cnt.add(0);    // 0 = nulo
        ch.push_back({0, 0}); cnt.add(0);    // 1 = raíz
    }

    // ---- modificaciones (SIEMPRE sobre la última versión) ----
    int insert(U x, int times = 1) { return addPath(x, times); }
    int erase(U x, int times = 1) {
        if (count(latest(), x) < times) throw std::runtime_error("erase: no hay suficientes copias");
        return addPath(x, -times);
    }

    // ---- consultas sobre cualquier versión v, o sobre (vr - vl) ----
    int size(int v) const { check(v); return C(v, ROOT); }
    int count(int v, U x) const {
        check(v);
        int a = ROOT;
        for (int b = B - 1; b >= 0 && a; --b) a = ch[a][(x >> b) & 1];
        return C(v, a);
    }
    U maxXor(int vl, int vr, U x) const {
        check(vl); check(vr);
        int a = ROOT;
        if (c(vl, vr, a) <= 0) throw std::runtime_error("maxXor: conjunto vacio");
        U res = 0;
        for (int b = B - 1; b >= 0; --b) {
            int want = ((x >> b) & 1) ^ 1;
            if (c(vl, vr, ch[a][want]) > 0) { res |= U(1) << b; a = ch[a][want]; }
            else a = ch[a][want ^ 1];
        }
        return res;
    }
    U minXor(int vl, int vr, U x) const {
        check(vl); check(vr);
        int a = ROOT;
        if (c(vl, vr, a) <= 0) throw std::runtime_error("minXor: conjunto vacio");
        U res = 0;
        for (int b = B - 1; b >= 0; --b) {
            int want = (x >> b) & 1;
            if (c(vl, vr, ch[a][want]) > 0) a = ch[a][want];
            else { res |= U(1) << b; a = ch[a][want ^ 1]; }
        }
        return res;
    }
    U kth(int vl, int vr, int k) const {
        check(vl); check(vr);
        int a = ROOT;
        if (k < 1 || k > c(vl, vr, a)) throw std::out_of_range("kth fuera de rango");
        U res = 0;
        for (int b = B - 1; b >= 0; --b) {
            int left = c(vl, vr, ch[a][0]);
            if (k <= left) a = ch[a][0];
            else { k -= left; res |= U(1) << b; a = ch[a][1]; }
        }
        return res;
    }
    int countLess(int vl, int vr, U x) const {
        check(vl); check(vr);
        int a = ROOT, res = 0;
        for (int b = B - 1; b >= 0 && a; --b) {
            int bit = (x >> b) & 1;
            if (bit) res += c(vl, vr, ch[a][0]);
            a = ch[a][bit];
        }
        return res;
    }
    U maxXor(int v, U x) const { return maxXor(0, v, x); }
    U minXor(int v, U x) const { return minXor(0, v, x); }
    U kth(int v, int k) const { return kth(0, v, k); }
    int countLess(int v, U x) const { return countLess(0, v, x); }

    int versions() const { return cnt.version() + 1; }
    int latest() const { return cnt.version(); }
    size_t entries() const { return cnt.entries(); }
    size_t nodes() const { return ch.size(); }
};

// ---------------------------------------------------------------------------
// 2) Trie de cadenas parcialmente persistente
// ---------------------------------------------------------------------------
template <int SIGMA = 26, char BASE = 'a'>
class PartialStringTrie {
    struct PE { int pass = 0, end = 0; };
    std::vector<std::array<int, SIGMA>> ch;   // hijos; se escriben una vez
    FatArray<PE> t;                           // (pass, end) con historial
    static constexpr int ROOT = 1;            // 0 = nulo

    void check(int v) const {
        if (v < 0 || v > t.version()) throw std::out_of_range("version invalida");
    }
    static int id(char c) {
        int k = (unsigned char)c - (unsigned char)BASE;
        if (k < 0 || k >= SIGMA) throw std::invalid_argument("caracter fuera del alfabeto");
        return k;
    }
    PE at(int v, int x) const { return x ? t.get(v, x) : PE{}; }
    int addPath(const std::string& s, int d) {
        for (char c : s) id(c);                  // valida antes de crear la versión
        t.newVersion();
        int a = ROOT;
        t.mut(a).pass += d;
        for (char c : s) {
            int k = id(c);
            if (!ch[a][k]) {
                int y = (int)ch.size();
                ch.push_back({});
                t.add(PE{});
                ch[a][k] = y;
            }
            a = ch[a][k];
            t.mut(a).pass += d;
        }
        t.mut(a).end += d;
        return latest();
    }
    int walk(const std::string& s) const {
        int a = ROOT;
        for (char c : s) { if (!a) return 0; a = ch[a][id(c)]; }
        return a;
    }

public:
    explicit PartialStringTrie(size_t reserveNodes = 0) {
        ch.reserve(reserveNodes + 2);
        t.reserve(reserveNodes + 2);
        ch.push_back({}); t.add(PE{});           // 0 = nulo
        ch.push_back({}); t.add(PE{});           // 1 = raíz
    }

    int insert(const std::string& s) { return addPath(s, +1); }
    int erase(const std::string& s) {
        if (count(latest(), s) == 0) throw std::runtime_error("erase: la palabra no existe");
        return addPath(s, -1);
    }

    int count(int v, const std::string& s) const { check(v); return at(v, walk(s)).end; }
    int countPrefix(int v, const std::string& p) const { check(v); return at(v, walk(p)).pass; }
    int size(int v) const { check(v); return at(v, ROOT).pass; }

    std::string kth(int v, int k) const {
        check(v);
        int a = ROOT;
        if (k < 1 || k > at(v, a).pass) throw std::out_of_range("kth fuera de rango");
        std::string res;
        while (true) {
            PE cur = at(v, a);
            if (k <= cur.end) return res;
            k -= cur.end;
            for (int c = 0; c < SIGMA; ++c) {
                int b = ch[a][c];
                int p = at(v, b).pass;
                if (k <= p) { res.push_back(char(BASE + c)); a = b; break; }
                k -= p;
            }
        }
    }

    int versions() const { return t.version() + 1; }
    int latest() const { return t.version(); }
    size_t entries() const { return t.entries(); }
    size_t nodes() const { return ch.size(); }
};
