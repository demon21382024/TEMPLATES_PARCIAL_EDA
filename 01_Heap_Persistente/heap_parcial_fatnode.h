#pragma once
// ============================================================================
//  HEAP PARCIALMENTE PERSISTENTE  -  heap binario de arreglo + FAT NODE
// ----------------------------------------------------------------------------
//  Persistencia PARCIAL: se puede CONSULTAR cualquier versión, pero solo se
//  MODIFICA la última (las versiones forman una línea 0,1,2,...).
//
//  Técnica fat node (Driscoll, Sarnak, Sleator, Tarjan 1989): en vez de copiar
//  nodos, cada celda guarda su historial [(versión, valor), ...]. Una
//  modificación agrega una entrada; leer la versión v es una búsqueda binaria.
//
//  El heap binario en arreglo NO sirve con path copying (copiar el arreglo es
//  O(n)), pero con fat node es directo: cada swap de sift-up/down agrega una
//  entrada a una celda.
//
//  Complejidades (m = # de modificaciones de una celda):
//    push / pop (última versión) : O(log n) tiempo, O(log n) memoria
//    top(v)                      : O(log m)   (O(1) en la última versión)
//    size(v)                     : O(1)
//  NO es funcional: las celdas (nodos) se modifican agregando historial.
// ============================================================================
#include <vector>
#include <functional>
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

template <class T, class Compare = std::less<T>>
class PartialPersistentHeap {
    FatArray<T> a;               // a[0..n-1] = heap binario de la versión actual
    std::vector<int> size_;      // tamaño de cada versión
    Compare cmp;

    void check(int v) const {
        if (v < 0 || v >= (int)size_.size()) throw std::out_of_range("version invalida");
    }

public:
    explicit PartialPersistentHeap(size_t reserveCells = 0, Compare c = Compare()) : cmp(c) {
        a.reserve(reserveCells);
        size_.push_back(0);                  // versión 0 = vacío
    }

    // ---- modificaciones (SIEMPRE sobre la última versión) ----
    int push(const T& x) {
        a.newVersion();
        int n = size_.back();
        if (n == a.size()) a.add(x);
        int i = n;                           // sift-up con "hueco"
        while (i > 0) {
            int p = (i - 1) / 2;
            if (!cmp(x, a.get(p))) break;
            a.set(i, a.get(p));
            i = p;
        }
        a.set(i, x);
        size_.push_back(n + 1);
        return latest();
    }

    int pop() {
        int n = size_.back();
        if (!n) throw std::runtime_error("pop en heap vacio");
        a.newVersion();
        T last = a.get(n - 1);
        --n;
        if (n > 0) {                         // sift-down con "hueco" desde la raíz
            int i = 0;
            while (true) {
                int l = 2 * i + 1;
                if (l >= n) break;
                int c = (l + 1 < n && cmp(a.get(l + 1), a.get(l))) ? l + 1 : l;
                if (!cmp(a.get(c), last)) break;
                a.set(i, a.get(c));
                i = c;
            }
            a.set(i, last);
        }
        size_.push_back(n);
        return latest();
    }

    // ---- consultas sobre CUALQUIER versión ----
    T top(int v) const {
        check(v);
        if (!size_[v]) throw std::runtime_error("top en heap vacio");
        return a.get(v, 0);
    }
    int  size(int v)  const { check(v); return size_[v]; }
    bool empty(int v) const { return size(v) == 0; }

    // k mejores de la versión v, en orden: O(k log k · log m)
    std::vector<T> topK(int v, int k) const {
        check(v);
        std::vector<T> out;
        std::vector<std::pair<T, int>> pq;   // (valor, índice)
        auto worse = [&](const std::pair<T, int>& x, const std::pair<T, int>& y) { return cmp(y.first, x.first); };
        if (size_[v]) pq.push_back({a.get(v, 0), 0});
        while (!pq.empty() && (int)out.size() < k) {
            std::pop_heap(pq.begin(), pq.end(), worse);
            auto [val, i] = pq.back(); pq.pop_back();
            out.push_back(val);
            for (int c : {2 * i + 1, 2 * i + 2})
                if (c < size_[v]) { pq.push_back({a.get(v, c), c}); std::push_heap(pq.begin(), pq.end(), worse); }
        }
        return out;
    }

    int versions() const { return (int)size_.size(); }
    int latest()   const { return versions() - 1; }
    size_t entries() const { return a.entries(); }   // memoria usada (entradas de historial)
};
