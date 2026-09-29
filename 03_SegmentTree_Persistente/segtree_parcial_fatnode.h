#pragma once
// ============================================================================
//  SEGMENT TREE PARCIALMENTE PERSISTENTE  -  árbol en arreglo + FAT NODE
// ----------------------------------------------------------------------------
//  Persistencia PARCIAL: se CONSULTA cualquier versión, solo se MODIFICA la
//  última (versiones en línea 0,1,2,...).
//
//  Técnica fat node (Driscoll et al.): el segment tree clásico en arreglo
//  (nodo i, hijos 2i y 2i+1, tamaño potencia de 2). Cada celda guarda su
//  historial [(versión, valor)]; un update agrega una entrada en cada nodo del
//  camino hoja -> raíz. Leer la versión v = búsqueda binaria en cada celda.
//
//  Plantillas:
//   1) PartialSegTree<Monoid>        set/apply O(log n) ; query(v,l,r) O(log n · log m)
//                                    kth(vl,vr,k) O(log n · log m) (árbol de conteo)
//   2) PartialRangeAddSegTree<T>     add(l,r,d) O(log n) ; query(v,l,r) O(log n · log m)
//                                    (lazy permanente, igual que la versión total)
//  Memoria: O(log n) entradas por update (misma asintótica que path copying,
//  pero sin punteros l/r: cada entrada es (versión, valor)).
//  NO es funcional: las celdas se modifican agregando historial.
//  Índices 0-based, rangos cerrados [l, r].
// ============================================================================
#include <vector>
#include <stdexcept>
#include <algorithm>
#include <limits>

#ifndef EDA_MONOIDES
#define EDA_MONOIDES
// --------------------------- Monoides de ejemplo ----------------------------
template <class X> struct SumM {
    using T = X;
    static T id() { return T(0); }
    static T op(const T& a, const T& b) { return a + b; }
};
template <class X> struct MinM {
    using T = X;
    static T id() { return std::numeric_limits<T>::max(); }
    static T op(const T& a, const T& b) { return std::min(a, b); }
};
template <class X> struct MaxM {
    using T = X;
    static T id() { return std::numeric_limits<T>::lowest(); }
    static T op(const T& a, const T& b) { return std::max(a, b); }
};
#endif

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
// 1) Actualización puntual (iterativo, abajo -> arriba)
// ---------------------------------------------------------------------------
template <class M>
class PartialSegTree {
public:
    using T = typename M::T;
private:
    int n, sz;
    FatArray<T> t;               // t[1..2sz-1]; hojas en [sz, 2sz)

    void check(int v) const {
        if (v < 0 || v > t.version()) throw std::out_of_range("version invalida");
    }
    void checkPos(int p) const {
        if (p < 0 || p >= n) throw std::out_of_range("posicion invalida");
    }
    void init(const std::vector<T>& a) {
        sz = 1;
        while (sz < n) sz <<= 1;
        t.reserve(2 * sz);
        for (int i = 0; i < 2 * sz; ++i) t.add(M::id());
        for (int i = 0; i < n; ++i) t.set(sz + i, a[i]);
        for (int i = sz - 1; i >= 1; --i) t.set(i, M::op(t.get(2 * i), t.get(2 * i + 1)));
    }

public:
    explicit PartialSegTree(int n_) : n(n_) { init(std::vector<T>(n, M::id())); }   // versión 0 = identidad
    explicit PartialSegTree(const std::vector<T>& a) : n((int)a.size()) { init(a); } // versión 0 = a

    // ---- modificaciones (SIEMPRE sobre la última versión) ----
    int set(int p, const T& val) {
        checkPos(p);
        t.newVersion();
        int i = p + sz;
        t.set(i, val);
        for (i >>= 1; i >= 1; i >>= 1) t.set(i, M::op(t.get(2 * i), t.get(2 * i + 1)));
        return latest();
    }
    int apply(int p, const T& val) { checkPos(p); return set(p, M::op(t.get(p + sz), val)); }

    // ---- consultas sobre cualquier versión ----
    T query(int v, int l, int r) const {
        check(v);
        if (l > r) return M::id();
        checkPos(l); checkPos(r);
        T resl = M::id(), resr = M::id();       // respeta el orden (op no conmutativa)
        for (l += sz, r += sz + 1; l < r; l >>= 1, r >>= 1) {
            if (l & 1) resl = M::op(resl, t.get(v, l++));
            if (r & 1) resr = M::op(t.get(v, --r), resr);
        }
        return M::op(resl, resr);
    }
    T get(int v, int p) const { check(v); checkPos(p); return t.get(v, p + sz); }

    // Árbol de CONTEO (M = SumM): k-ésimo menor entre versiones vl < vr, o -1.
    int kth(int vl, int vr, T k) const {
        check(vl); check(vr);
        if (k < T(1) || t.get(vr, 1) - t.get(vl, 1) < k) return -1;
        int i = 1;
        while (i < sz) {
            T left = t.get(vr, 2 * i) - t.get(vl, 2 * i);
            if (k <= left) i = 2 * i;
            else { k -= left; i = 2 * i + 1; }
        }
        return i - sz;
    }

    int versions() const { return t.version() + 1; }
    int latest() const { return t.version(); }
    size_t entries() const { return t.entries(); }
    int length() const { return n; }
};

// ---------------------------------------------------------------------------
// 2) Suma en rango + suma de rango (lazy permanente)
// ---------------------------------------------------------------------------
template <class T = long long>
class PartialRangeAddSegTree {
    struct Node { T sum = T(0), add = T(0); };   // igual que en la versión total
    int n, sz;
    FatArray<Node> t;

    void check(int v) const {
        if (v < 0 || v > t.version()) throw std::out_of_range("version invalida");
    }
    void checkRange(int l, int r) const {
        if (l < 0 || r >= n || l > r) throw std::out_of_range("rango invalido");
    }
    void upd(int x, int lo, int hi, int l, int r, const T& d) {
        Node& nd = t.mut(x);                      // las llamadas recursivas tocan OTRAS celdas
        nd.sum += d * T(std::min(r, hi) - std::max(l, lo) + 1);
        if (l <= lo && hi <= r) { nd.add += d; return; }
        int mid = (lo + hi) >> 1;
        if (l <= mid) upd(2 * x, lo, mid, l, r, d);
        if (r > mid)  upd(2 * x + 1, mid + 1, hi, l, r, d);
    }
    T qry(int v, int x, int lo, int hi, int l, int r, T acc) const {
        Node nd = t.get(v, x);
        if (l <= lo && hi <= r) return nd.sum + acc * T(hi - lo + 1);
        acc += nd.add;
        int mid = (lo + hi) >> 1;
        T res = T(0);
        if (l <= mid) res += qry(v, 2 * x, lo, mid, l, r, acc);
        if (r > mid)  res += qry(v, 2 * x + 1, mid + 1, hi, l, r, acc);
        return res;
    }
    void init(const std::vector<T>& a) {
        sz = 1;
        while (sz < n) sz <<= 1;
        t.reserve(2 * sz);
        for (int i = 0; i < 2 * sz; ++i) t.add(Node{});
        for (int i = 0; i < n; ++i) t.mut(sz + i).sum = a[i];
        for (int i = sz - 1; i >= 1; --i) t.mut(i).sum = t.get(2 * i).sum + t.get(2 * i + 1).sum;
    }

public:
    explicit PartialRangeAddSegTree(int n_) : n(n_) { init(std::vector<T>(n, T(0))); }
    explicit PartialRangeAddSegTree(const std::vector<T>& a) : n((int)a.size()) { init(a); }

    int add(int l, int r, const T& d) {
        checkRange(l, r);
        t.newVersion();
        upd(1, 0, sz - 1, l, r, d);
        return latest();
    }
    T query(int v, int l, int r) const {
        check(v); checkRange(l, r);
        return qry(v, 1, 0, sz - 1, l, r, T(0));
    }
    T get(int v, int p) const { return query(v, p, p); }

    int versions() const { return t.version() + 1; }
    int latest() const { return t.version(); }
    size_t entries() const { return t.entries(); }
    int length() const { return n; }
};
