#pragma once
// ============================================================================
//  STACK PARCIALMENTE PERSISTENTE  -  pila en arreglo + FAT NODE
// ----------------------------------------------------------------------------
//  Persistencia PARCIAL: se CONSULTA cualquier versión, solo se MODIFICA la
//  última (versiones en línea 0,1,2,...).
//
//  Técnica fat node (Driscoll et al.): la pila clásica en arreglo
//  (a[0..n-1], tope = a[n-1]). Cada celda guarda su historial
//  [(versión, valor)] y el tamaño se guarda por versión. push escribe UNA
//  celda; pop no escribe ninguna (solo cambia el tamaño). Leer la celda i en
//  la versión v = búsqueda binaria en el historial de i.
//
//  Complejidades (m = # de veces que se sobrescribió una celda):
//    push / pop (última versión) : O(1) tiempo y memoria
//    top(v), kth(v, k)           : O(log m)   (O(1) en la última versión)
//    size(v)                     : O(1)
//  Ventaja sobre la lista enlazada: acceso ALEATORIO kth(v, k) sin jump
//  pointers, y los datos quedan contiguos en memoria.
//  NO es funcional: las celdas se modifican agregando historial.
// ============================================================================
#include <vector>
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

template <class T>
class PartialStack {
    FatArray<T> a;              // a[0..size-1] = pila de la versión actual
    std::vector<int> size_;     // tamaño de cada versión

    void check(int v) const {
        if (v < 0 || v >= (int)size_.size()) throw std::out_of_range("version invalida");
    }

public:
    explicit PartialStack(size_t reserveCells = 0) {
        a.reserve(reserveCells);
        size_.push_back(0);                  // versión 0 = vacía
    }

    // ---- modificaciones (SIEMPRE sobre la última versión) ----
    int push(const T& x) {
        a.newVersion();
        int n = size_.back();
        if (n == a.size()) a.add(x);
        else a.set(n, x);
        size_.push_back(n + 1);
        return latest();
    }
    int pop() {
        if (!size_.back()) throw std::runtime_error("pop en pila vacia");
        a.newVersion();                      // el arreglo no cambia: solo el tamaño
        size_.push_back(size_.back() - 1);
        return latest();
    }

    // ---- consultas sobre cualquier versión ----
    T top(int v) const {
        check(v);
        if (!size_[v]) throw std::runtime_error("top en pila vacia");
        return a.get(v, size_[v] - 1);
    }
    int  size(int v)  const { check(v); return size_[v]; }
    bool empty(int v) const { return size(v) == 0; }
    // k-ésimo desde el tope (k = 0 es el tope)
    T kth(int v, int k) const {
        check(v);
        if (k < 0 || k >= size_[v]) throw std::out_of_range("kth fuera de rango");
        return a.get(v, size_[v] - 1 - k);
    }
    std::vector<T> toVector(int v) const {   // del tope al fondo
        check(v);
        std::vector<T> out;
        for (int i = size_[v] - 1; i >= 0; --i) out.push_back(a.get(v, i));
        return out;
    }

    int versions() const { return (int)size_.size(); }
    int latest() const { return versions() - 1; }
    size_t entries() const { return a.entries(); }
};
