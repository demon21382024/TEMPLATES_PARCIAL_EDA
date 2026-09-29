#pragma once
// ============================================================================
//  FIBONACCI HEAP PARCIALMENTE PERSISTENTE  -  CLRS + FAT NODE
// ----------------------------------------------------------------------------
//  Persistencia PARCIAL: se CONSULTA cualquier versión, solo se MODIFICA la
//  última (versiones en línea 0,1,2,...).
//
//  Técnica fat node (Driscoll et al.): el Fibonacci heap es el CLÁSICO de CLRS
//  (lista circular de raíces, listas circulares de hijos, marcas, cut,
//  cascading cut). Cada registro (nodo) guarda su historial
//  [(versión, registro)]: modificar un nodo en la versión actual agrega UNA
//  entrada (o pisa la de esta versión). El mínimo y el tamaño se guardan por
//  versión. Los ciclos no molestan: no hay caminos que copiar.
//
//  Complejidades (sobre la última versión = las del Fibonacci heap efímero,
//  porque leer la versión actual es O(1)):
//    insert, getMin         O(1)
//    decreaseKey            O(1) amortizado
//    extractMin, erase      O(log n) amortizado
//  Consultas en una versión vieja v: cada lectura de nodo cuesta O(log m)
//  (búsqueda binaria en su historial):  getMin(v), keyOf(v,h) O(log m).
//  Memoria: O(1) por campo modificado => ~2.7x menos que la versión total
//  (medido: 100k insert + 100k extractMin = 138 MB vs 366 MB).
//  Sin merge: combinar versiones requiere persistencia confluente.
//  NO es funcional: los nodos se modifican agregando historial.
//
//  Requisito: T con constructor por defecto.
//  Compare = std::less<T> -> min-heap ; std::greater<T> -> max-heap
// ============================================================================
#include <vector>
#include <functional>
#include <stdexcept>
#include <utility>
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

template <class T, class Compare = std::less<T>>
class PartialFibonacciHeap {
public:
    using Handle = int;          // índice del nodo; 0 = nulo

private:
    struct Rec {
        T    key{};
        int  parent = 0, child = 0;      // child = algún hijo (lista circular)
        int  left = 0, right = 0;        // hermanos (lista circular)
        int  degree = 0;
        bool mark = false;
        bool alive = false;              // está en el heap
    };
    FatArray<Rec> a;                     // a[0] = nulo
    std::vector<int> mn_, n_;            // mínimo y tamaño de cada versión
    int mn = 0, n = 0;                   // estado de la última versión
    Compare cmp;
    std::vector<int> buf;

    // R(x): lectura en la versión actual (O(1)). La referencia se invalida si
    // luego se escribe el MISMO nodo con W(x): por eso se copian los campos.
    const Rec& R(int x) const { return a.get(x); }
    Rec& W(int x) { return a.mut(x); }

    void check(int v) const {
        if (v < 0 || v >= (int)mn_.size()) throw std::out_of_range("version invalida");
    }
    bool aliveNow(Handle h) const { return h > 0 && h < a.size() && R(h).alive; }
    int commit() { mn_.push_back(mn); n_.push_back(n); return latest(); }

    // agrega x a la lista circular de raíces (junto a mn)
    void addRoot(int x) {
        if (!mn) {
            Rec& w = W(x);
            w.left = w.right = x; w.parent = 0; w.mark = false;
            mn = x;
            return;
        }
        int mr = R(mn).right;
        { Rec& w = W(x); w.left = mn; w.right = mr; w.parent = 0; w.mark = false; }
        W(mr).left = x;
        W(mn).right = x;
    }
    // y (raíz) pasa a ser hijo de x (raíz)
    void link(int y, int x) {
        int c = R(x).child;
        if (!c) {
            { Rec& w = W(y); w.parent = x; w.left = w.right = y; w.mark = false; }
            W(x).child = y;
        } else {
            int cr = R(c).right;
            { Rec& w = W(y); w.parent = x; w.left = c; w.right = cr; w.mark = false; }
            W(cr).left = y;
            W(c).right = y;
        }
        W(x).degree++;
    }
    // une árboles de igual grado y reconstruye la lista circular de raíces
    void consolidate(const std::vector<int>& cand) {
        int A[64] = {0};                 // grado máx. <= log_phi(n) < 64
        T   K[64];
        int maxd = -1;
        for (int w : cand) {
            int x = w;
            T kx = R(x).key;
            int d = R(x).degree;
            while (A[d]) {
                int y = A[d];
                T ky = K[d];
                if (cmp(ky, kx)) { std::swap(x, y); std::swap(kx, ky); }
                link(y, x);
                A[d] = 0;
                ++d;
            }
            A[d] = x; K[d] = kx;
            if (d > maxd) maxd = d;
        }
        std::vector<int> rs;
        int best = -1;
        for (int d = 0; d <= maxd; ++d)
            if (A[d]) { rs.push_back(A[d]); if (best < 0 || cmp(K[d], K[best])) best = d; }
        int k = (int)rs.size();
        for (int i = 0; i < k; ++i) {
            Rec& w = W(rs[i]);
            w.left = rs[(i + k - 1) % k]; w.right = rs[(i + 1) % k];
            w.parent = 0; w.mark = false;
        }
        mn = best < 0 ? 0 : A[best];
    }
    // elimina mn de la versión actual
    void extractCore() {
        int z = mn;
        buf.clear();
        for (int x = R(z).right; x != z; x = R(x).right) buf.push_back(x);   // otras raíces
        int c = R(z).child;
        if (c) { int x = c; do { buf.push_back(x); x = R(x).right; } while (x != c); }  // hijos
        { Rec& w = W(z); w.alive = false; w.child = 0; w.parent = 0; w.degree = 0; }
        --n;
        consolidate(buf);
    }
    void cut(int x, int y) {
        int xl = R(x).left, xr = R(x).right;
        if (xr == x) W(y).child = 0;
        else {
            W(xl).right = xr;
            W(xr).left = xl;
            if (R(y).child == x) W(y).child = xr;
        }
        W(y).degree--;
        addRoot(x);
    }
    void cascadingCut(int y) {
        while (true) {
            int z = R(y).parent;
            if (!z) return;
            if (!R(y).mark) { W(y).mark = true; return; }
            cut(y, z);
            y = z;
        }
    }

public:
    explicit PartialFibonacciHeap(size_t reserveNodes = 0, Compare c = Compare()) : cmp(c) {
        a.reserve(reserveNodes + 1);
        a.add(Rec{});                    // 0 = nulo
        mn_.push_back(0); n_.push_back(0);   // versión 0 = vacío
    }

    // ---- modificaciones (SIEMPRE sobre la última versión) ----
    int insert(const T& k, Handle* h = nullptr) {
        a.newVersion();
        Rec r; r.key = k; r.alive = true;
        int id = a.add(r);
        int old = mn;
        addRoot(id);
        if (old && cmp(k, R(old).key)) mn = id;
        ++n;
        if (h) *h = id;
        return commit();
    }
    int extractMin() {
        if (!mn) throw std::runtime_error("extractMin en heap vacio");
        a.newVersion();
        extractCore();
        return commit();
    }
    int decreaseKey(Handle h, const T& k) {
        if (!aliveNow(h)) throw std::invalid_argument("decreaseKey: el nodo no esta en el heap");
        if (cmp(R(h).key, k)) throw std::invalid_argument("decreaseKey: la nueva clave es peor");
        a.newVersion();
        W(h).key = k;
        int y = R(h).parent;
        if (y && cmp(k, R(y).key)) { cut(h, y); cascadingCut(y); }
        if (cmp(k, R(mn).key)) mn = h;
        return commit();
    }
    int erase(Handle h) {
        if (!aliveNow(h)) throw std::invalid_argument("erase: el nodo no esta en el heap");
        a.newVersion();
        int y = R(h).parent;
        if (y) { cut(h, y); cascadingCut(y); }
        mn = h;                          // equivale a decreaseKey(h, -inf) + extractMin
        extractCore();
        return commit();
    }

    // ---- consultas sobre cualquier versión ----
    T getMin(int v) const {
        check(v);
        if (!mn_[v]) throw std::runtime_error("getMin en heap vacio");
        return a.get(v, mn_[v]).key;
    }
    Handle minHandle(int v) const { check(v); return mn_[v]; }
    int  size(int v)  const { check(v); return n_[v]; }
    bool empty(int v) const { return size(v) == 0; }
    bool contains(int v, Handle h) const { check(v); return h > 0 && h < a.size() && a.get(v, h).alive; }
    T keyOf(int v, Handle h) const {
        if (!contains(v, h)) throw std::invalid_argument("keyOf: el nodo no esta en esta version");
        return a.get(v, h).key;
    }
    // todas las claves de la versión v (O(n log m)); útil para depurar
    std::vector<T> elements(int v) const {
        check(v);
        std::vector<T> out;
        if (!mn_[v]) return out;
        std::vector<int> st = {mn_[v]};  // cada elemento = inicio de una lista circular
        while (!st.empty()) {
            int start = st.back(); st.pop_back();
            int x = start;
            do {
                Rec r = a.get(v, x);
                out.push_back(r.key);
                if (r.child) st.push_back(r.child);
                x = r.right;
            } while (x != start);
        }
        return out;
    }

    int versions() const { return (int)mn_.size(); }
    int latest() const { return versions() - 1; }
    size_t entries() const { return a.entries(); }
    // bytes usados (aprox.): entradas de historial + vectores por nodo + versiones
    size_t memoryBytes() const {
        return a.entries() * (sizeof(int) + sizeof(Rec)) + (size_t)a.size() * sizeof(std::vector<int>)
             + mn_.size() * 2 * sizeof(int);
    }
};
