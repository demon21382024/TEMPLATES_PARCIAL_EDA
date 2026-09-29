#pragma once
// ============================================================================
//  SEGMENT TREE PERSISTENTE  (path copying)
// ----------------------------------------------------------------------------
//  Una actualización puntual copia solo los O(log n) nodos del camino
//  raíz -> hoja; los demás subárboles se comparten con la versión anterior.
//
//  Nodo 0 = nulo: su valor es la identidad y sus hijos son él mismo. Por eso
//  un árbol "todo identidad" (p. ej. todo ceros) cuesta 0 memoria y la versión
//  inicial es gratis: solo se crean nodos al actualizar (árbol implícito).
//
//  Plantillas:
//
//  1) PersistentSegTree<Monoid>      actualización puntual + consulta de rango
//       set / apply   : O(log n) tiempo y memoria
//       query / get   : O(log n)
//       kth(vl,vr,k)  : O(log n)  (solo si T es numérico con resta: árbol de
//                       conteo; responde "k-ésimo menor en a[l..r]")
//     Monoid debe definir:  using T;  static T id();  static T op(const T&, const T&)
//
//  2) PersistentRangeAddSegTree<T>   suma en rango + suma de rango
//       add(v,l,r,d)  : O(log n) tiempo y memoria
//       query(v,l,r)  : O(log n)
//     Usa "lazy permanente" (marca que NO se propaga): propagar obligaría a
//     copiar hijos en cada consulta; así las consultas no crean nodos.
//
//  Índices 0-based, rangos cerrados [l, r].
// ============================================================================
#include <vector>
#include <stdexcept>
#include <algorithm>
#include <limits>

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

// ---------------------------------------------------------------------------
// 1) Actualización puntual
// ---------------------------------------------------------------------------
template <class M>
class PersistentSegTree {
public:
    using T = typename M::T;
private:
    struct Node { T val; int l, r; };
    int n;
    std::vector<Node> t;
    std::vector<int> root_;

    int make(T val, int l, int r) {
        t.push_back(Node{std::move(val), l, r});
        return (int)t.size() - 1;
    }
    int build(const std::vector<T>& a, int lo, int hi) {
        if (lo == hi) return make(a[lo], 0, 0);
        int mid = (lo + hi) >> 1;
        int L = build(a, lo, mid), R = build(a, mid + 1, hi);
        return make(M::op(t[L].val, t[R].val), L, R);
    }
    int upd(int x, int lo, int hi, int p, const T& v, bool assign) {
        if (lo == hi) return make(assign ? v : M::op(t[x].val, v), 0, 0);
        int mid = (lo + hi) >> 1;
        int L = t[x].l, R = t[x].r;
        if (p <= mid) L = upd(L, lo, mid, p, v, assign);
        else          R = upd(R, mid + 1, hi, p, v, assign);
        return make(M::op(t[L].val, t[R].val), L, R);
    }
    T qry(int x, int lo, int hi, int l, int r) const {
        // x == 0: subárbol nulo = todo identidad
        if (!x || r < lo || hi < l) return M::id();
        if (l <= lo && hi <= r) return t[x].val;
        int mid = (lo + hi) >> 1;
        return M::op(qry(t[x].l, lo, mid, l, r), qry(t[x].r, mid + 1, hi, l, r));
    }

    int newVersion(int r) { root_.push_back(r); return (int)root_.size() - 1; }
    void check(int v) const {
        if (v < 0 || v >= (int)root_.size()) throw std::out_of_range("version invalida");
    }
    void checkPos(int p) const {
        if (p < 0 || p >= n) throw std::out_of_range("posicion invalida");
    }

public:
    // Versión 0 = arreglo de tamaño n lleno de M::id()  (sin memoria)
    explicit PersistentSegTree(int n_, size_t reserveNodes = 0) : n(n_) {
        t.reserve(reserveNodes + 1);
        t.push_back(Node{M::id(), 0, 0});
        newVersion(0);
    }
    // Versión 0 = arreglo a  (2n-1 nodos)
    explicit PersistentSegTree(const std::vector<T>& a, size_t reserveNodes = 0) : n((int)a.size()) {
        t.reserve(std::max(reserveNodes, 2 * a.size()) + 1);
        t.push_back(Node{M::id(), 0, 0});
        newVersion(n ? build(a, 0, n - 1) : 0);
    }

    // ---- modificaciones: devuelven la nueva versión ----
    int set(int v, int p, const T& val)   { check(v); checkPos(p); return newVersion(upd(root_[v], 0, n - 1, p, val, true)); }
    int apply(int v, int p, const T& val) { check(v); checkPos(p); return newVersion(upd(root_[v], 0, n - 1, p, val, false)); }

    // ---- consultas ----
    T query(int v, int l, int r) const {
        check(v);
        if (l > r) return M::id();
        checkPos(l); checkPos(r);
        return qry(root_[v], 0, n - 1, l, r);
    }
    T get(int v, int p) const { return query(v, p, p); }

    // Árbol de CONTEO (M = SumM): la versión i contiene las frecuencias de los
    // valores (comprimidos) de a[0..i-1]. Devuelve el índice (valor comprimido)
    // del k-ésimo menor (k desde 1) entre las versiones vl < vr, o -1 si no hay.
    int kth(int vl, int vr, T k) const {
        check(vl); check(vr);
        int a = root_[vr], b = root_[vl], lo = 0, hi = n - 1;
        if (k < T(1) || t[a].val - t[b].val < k) return -1;
        while (lo < hi) {
            int mid = (lo + hi) >> 1;
            T left = t[t[a].l].val - t[t[b].l].val;
            if (k <= left) { a = t[a].l; b = t[b].l; hi = mid; }
            else { k -= left; a = t[a].r; b = t[b].r; lo = mid + 1; }
        }
        return lo;
    }

    int versions() const { return (int)root_.size(); }
    size_t nodes() const { return t.size(); }
    int length() const { return n; }
};

// ---------------------------------------------------------------------------
// 2) Suma en rango + suma de rango (lazy permanente)
// ---------------------------------------------------------------------------
template <class T = long long>
class PersistentRangeAddSegTree {
    // sum = suma real del segmento SIN contar las marcas de los ancestros
    // add = valor sumado a TODO el segmento (marca que no se propaga)
    struct Node { T sum, add; int l, r; };
    int n;
    std::vector<Node> t;
    std::vector<int> root_;

    int clone(int x) { Node c = t[x]; t.push_back(c); return (int)t.size() - 1; }
    int build(const std::vector<T>& a, int lo, int hi) {
        if (lo == hi) { t.push_back(Node{a[lo], T(0), 0, 0}); return (int)t.size() - 1; }
        int mid = (lo + hi) >> 1;
        int L = build(a, lo, mid), R = build(a, mid + 1, hi);
        t.push_back(Node{t[L].sum + t[R].sum, T(0), L, R});
        return (int)t.size() - 1;
    }
    int upd(int x, int lo, int hi, int l, int r, const T& d) {
        int nx = clone(x);
        t[nx].sum += d * T(std::min(r, hi) - std::max(l, lo) + 1);
        if (l <= lo && hi <= r) { t[nx].add += d; return nx; }
        int mid = (lo + hi) >> 1;
        if (l <= mid) { int c = upd(t[nx].l, lo, mid, l, r, d);     t[nx].l = c; }
        if (r > mid)  { int c = upd(t[nx].r, mid + 1, hi, l, r, d); t[nx].r = c; }
        return nx;
    }
    T qry(int x, int lo, int hi, int l, int r, T acc) const {
        if (l <= lo && hi <= r) return t[x].sum + acc * T(hi - lo + 1);
        acc += t[x].add;
        int mid = (lo + hi) >> 1;
        T res = T(0);
        if (l <= mid) res += qry(t[x].l, lo, mid, l, r, acc);
        if (r > mid)  res += qry(t[x].r, mid + 1, hi, l, r, acc);
        return res;
    }
    int newVersion(int r) { root_.push_back(r); return (int)root_.size() - 1; }
    void check(int v) const {
        if (v < 0 || v >= (int)root_.size()) throw std::out_of_range("version invalida");
    }
    void checkRange(int l, int r) const {
        if (l < 0 || r >= n || l > r) throw std::out_of_range("rango invalido");
    }

public:
    explicit PersistentRangeAddSegTree(int n_, size_t reserveNodes = 0) : n(n_) {   // todo ceros
        t.reserve(reserveNodes + 1);
        t.push_back(Node{T(0), T(0), 0, 0});
        newVersion(0);
    }
    explicit PersistentRangeAddSegTree(const std::vector<T>& a, size_t reserveNodes = 0) : n((int)a.size()) {
        t.reserve(std::max(reserveNodes, 2 * a.size()) + 1);
        t.push_back(Node{T(0), T(0), 0, 0});
        newVersion(n ? build(a, 0, n - 1) : 0);
    }

    int add(int v, int l, int r, const T& d) {
        check(v); checkRange(l, r);
        return newVersion(upd(root_[v], 0, n - 1, l, r, d));
    }
    T query(int v, int l, int r) const {
        check(v); checkRange(l, r);
        return qry(root_[v], 0, n - 1, l, r, T(0));
    }
    T get(int v, int p) const { return query(v, p, p); }

    int versions() const { return (int)root_.size(); }
    size_t nodes() const { return t.size(); }
    int length() const { return n; }
};
