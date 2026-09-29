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
//       merge(v1,v2)  : a[i] = op(a1[i], a2[i]); O(nodos solapados)
//     Monoid debe definir:  using T;  static T id();  static T op(const T&, const T&)
//
//  2) PersistentRangeAddSegTree<T>   suma en rango + suma de rango
//       add(v,l,r,d)  : O(log n) tiempo y memoria
//       query(v,l,r)  : O(log n)
//       merge(v1,v2)  : a[i] = a1[i] + a2[i]; O(nodos solapados)
//     Usa "lazy permanente" (marca que NO se propaga): propagar obligaría a
//     copiar hijos en cada consulta; así las consultas no crean nodos.
//
//  Índices 0-based, rangos cerrados [l, r].
//
//  Tipos de persistencia (modo del constructor):
//    Parcial    : updates solo sobre la última versión; merge prohibido.
//    Total      : updates sobre cualquier versión; merge prohibido.
//    Confluente : Total + merge(v1, v2) (combina elemento a elemento).
//    Funcional  : siempre (un nodo publicado jamás se modifica).
//  Consultar DOS versiones (kth con vl, vr) está permitido en todos los modos.
//  Para parcial con la técnica "fat node" ver segtree_parcial_fatnode.h
// ============================================================================
#include <vector>
#include <stdexcept>
#include <algorithm>
#include <limits>

#ifndef EDA_PERSISTENCIA
#define EDA_PERSISTENCIA
// Parcial    : se consultan todas las versiones, solo se modifica la última.
// Total      : se consulta y modifica cualquier versión (árbol de versiones).
// Confluente : Total + operaciones que combinan dos versiones (DAG de versiones).
enum class Persistencia { Parcial, Total, Confluente };
#endif

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
    Persistencia modo;

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
    // combina dos árboles elemento a elemento; un subárbol nulo es identidad,
    // así que se reutiliza el otro sin copiarlo
    int unite(int a, int b, int lo, int hi) {
        if (!a) return b;
        if (!b) return a;
        if (lo == hi) return make(M::op(t[a].val, t[b].val), 0, 0);
        int mid = (lo + hi) >> 1;
        int L = unite(t[a].l, t[b].l, lo, mid);
        int R = unite(t[a].r, t[b].r, mid + 1, hi);
        return make(M::op(t[L].val, t[R].val), L, R);
    }

    int newVersion(int r) { root_.push_back(r); return (int)root_.size() - 1; }
    void check(int v) const {
        if (v < 0 || v >= (int)root_.size()) throw std::out_of_range("version invalida");
    }
    void checkWrite(int v) const {
        check(v);
        if (modo == Persistencia::Parcial && v != versions() - 1)
            throw std::logic_error("persistencia parcial: solo se modifica la ultima version");
    }
    void checkConfluent() const {
        if (modo != Persistencia::Confluente)
            throw std::logic_error("combinar versiones requiere persistencia confluente");
    }
    void checkPos(int p) const {
        if (p < 0 || p >= n) throw std::out_of_range("posicion invalida");
    }

public:
    // Versión 0 = arreglo de tamaño n lleno de M::id()  (sin memoria)
    explicit PersistentSegTree(int n_, Persistencia m = Persistencia::Confluente, size_t reserveNodes = 0)
        : n(n_), modo(m) {
        t.reserve(reserveNodes + 1);
        t.push_back(Node{M::id(), 0, 0});
        newVersion(0);
    }
    // Versión 0 = arreglo a  (2n-1 nodos)
    explicit PersistentSegTree(const std::vector<T>& a, Persistencia m = Persistencia::Confluente,
                               size_t reserveNodes = 0) : n((int)a.size()), modo(m) {
        t.reserve(std::max(reserveNodes, 2 * a.size()) + 1);
        t.push_back(Node{M::id(), 0, 0});
        newVersion(n ? build(a, 0, n - 1) : 0);
    }

    // ---- modificaciones: devuelven la nueva versión ----
    int set(int v, int p, const T& val)   { checkWrite(v); checkPos(p); return newVersion(upd(root_[v], 0, n - 1, p, val, true)); }
    int apply(int v, int p, const T& val) { checkWrite(v); checkPos(p); return newVersion(upd(root_[v], 0, n - 1, p, val, false)); }
    // CONFLUENTE: nueva versión con a[i] = op(v1[i], v2[i])
    int merge(int v1, int v2) {
        checkConfluent(); check(v1); check(v2);
        return newVersion(n ? unite(root_[v1], root_[v2], 0, n - 1) : 0);
    }

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
    int latest() const { return versions() - 1; }
    Persistencia mode() const { return modo; }
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
    Persistencia modo;

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
    // suma elemento a elemento: sumas y marcas se suman nodo a nodo
    int unite(int a, int b) {
        if (!a) return b;
        if (!b) return a;
        int L = unite(t[a].l, t[b].l);
        int R = unite(t[a].r, t[b].r);
        t.push_back(Node{t[a].sum + t[b].sum, t[a].add + t[b].add, L, R});
        return (int)t.size() - 1;
    }
    int newVersion(int r) { root_.push_back(r); return (int)root_.size() - 1; }
    void check(int v) const {
        if (v < 0 || v >= (int)root_.size()) throw std::out_of_range("version invalida");
    }
    void checkWrite(int v) const {
        check(v);
        if (modo == Persistencia::Parcial && v != versions() - 1)
            throw std::logic_error("persistencia parcial: solo se modifica la ultima version");
    }
    void checkConfluent() const {
        if (modo != Persistencia::Confluente)
            throw std::logic_error("combinar versiones requiere persistencia confluente");
    }
    void checkRange(int l, int r) const {
        if (l < 0 || r >= n || l > r) throw std::out_of_range("rango invalido");
    }

public:
    explicit PersistentRangeAddSegTree(int n_, Persistencia m = Persistencia::Confluente, size_t reserveNodes = 0)
        : n(n_), modo(m) {                                                  // todo ceros
        t.reserve(reserveNodes + 1);
        t.push_back(Node{T(0), T(0), 0, 0});
        newVersion(0);
    }
    explicit PersistentRangeAddSegTree(const std::vector<T>& a, Persistencia m = Persistencia::Confluente,
                                       size_t reserveNodes = 0) : n((int)a.size()), modo(m) {
        t.reserve(std::max(reserveNodes, 2 * a.size()) + 1);
        t.push_back(Node{T(0), T(0), 0, 0});
        newVersion(n ? build(a, 0, n - 1) : 0);
    }

    int add(int v, int l, int r, const T& d) {
        checkWrite(v); checkRange(l, r);
        return newVersion(upd(root_[v], 0, n - 1, l, r, d));
    }
    // CONFLUENTE: nueva versión con a[i] = v1[i] + v2[i]
    int merge(int v1, int v2) {
        checkConfluent(); check(v1); check(v2);
        return newVersion(unite(root_[v1], root_[v2]));
    }
    T query(int v, int l, int r) const {
        check(v); checkRange(l, r);
        return qry(root_[v], 0, n - 1, l, r, T(0));
    }
    T get(int v, int p) const { return query(v, p, p); }

    int versions() const { return (int)root_.size(); }
    int latest() const { return versions() - 1; }
    Persistencia mode() const { return modo; }
    size_t nodes() const { return t.size(); }
    int length() const { return n; }
};
