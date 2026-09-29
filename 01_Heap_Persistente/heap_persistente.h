#pragma once
// ============================================================================
//  HEAP PERSISTENTE  -  Leftist Heap (montículo zurdo) con path copying
// ----------------------------------------------------------------------------
//  Idea:
//    * Leftist heap: cada nodo guarda s = distancia al nulo más cercano por la
//      derecha, y se mantiene s(izq) >= s(der). Así la espina derecha mide
//      O(log n) y MERGE baja solo por espinas derechas: O(log n).
//    * Persistencia: nunca se modifica un nodo; merge crea copias SOLO de los
//      nodos de la espina derecha recorrida (path copying). Todo lo demás se
//      comparte entre versiones.
//
//  Complejidades (n = tamaño de la versión):
//    push  : O(log n) tiempo, O(log n) memoria nueva
//    pop   : O(log n) tiempo, O(log n) memoria nueva
//    top   : O(1)
//    merge : O(log n + log m)   (funciona incluso fusionando una versión consigo misma)
//    build : O(n)
//
//  Memoria: nodos en un pool contiguo (vector) y punteros como índices int
//  (4 bytes en vez de 8). El nodo 0 es el nulo.
//  Requisito: T tiene constructor por defecto (se usa en el nodo nulo).
//  Compare = std::less<T>  -> min-heap ;  std::greater<T> -> max-heap
// ============================================================================
#include <vector>
#include <functional>
#include <stdexcept>
#include <utility>
#include <algorithm>

template <class T, class Compare = std::less<T>>
class PersistentHeap {
    struct Node { T key; int l, r, s; };

    std::vector<Node> pool;     // pool[0] = nulo (s = 0)
    std::vector<int>  root_;    // raíz de cada versión
    std::vector<int>  size_;    // tamaño de cada versión
    Compare cmp;

    // Crea un nodo nuevo manteniendo la propiedad zurda.
    // (key por valor: push_back puede realocar el pool)
    int make(T key, int l, int r) {
        if (pool[l].s < pool[r].s) std::swap(l, r);
        pool.push_back(Node{std::move(key), l, r, pool[r].s + 1});
        return (int)pool.size() - 1;
    }

    // Fusiona dos heaps (raíces a y b) SIN modificarlos.
    int meld(int a, int b) {
        if (!a) return b;
        if (!b) return a;
        if (cmp(pool[b].key, pool[a].key)) std::swap(a, b);  // a tiene la mejor clave
        int nr = meld(pool[a].r, b);                          // baja por la espina derecha
        return make(pool[a].key, pool[a].l, nr);              // copia de a
    }

    int newVersion(int root, int sz) {
        root_.push_back(root);
        size_.push_back(sz);
        return (int)root_.size() - 1;
    }
    void check(int v) const {
        if (v < 0 || v >= (int)root_.size()) throw std::out_of_range("version invalida");
    }

public:
    // reserveNodes: si conoces el nº aprox. de nodos, evita realocaciones.
    explicit PersistentHeap(size_t reserveNodes = 0, Compare c = Compare()) : cmp(c) {
        pool.reserve(reserveNodes + 1);
        pool.push_back(Node{T(), 0, 0, 0});
        newVersion(0, 0);                      // versión 0 = heap vacío
    }

    // ---- operaciones: todas devuelven el ID de la NUEVA versión ----
    int push(int v, const T& x) {
        check(v);
        int leaf = make(x, 0, 0);
        return newVersion(meld(root_[v], leaf), size_[v] + 1);
    }

    int pop(int v) {
        check(v);
        if (!root_[v]) throw std::runtime_error("pop en heap vacio");
        const Node& r = pool[root_[v]];
        return newVersion(meld(r.l, r.r), size_[v] - 1);   // no crea la raíz, solo fusiona hijos
    }

    int merge(int v1, int v2) {
        check(v1); check(v2);
        return newVersion(meld(root_[v1], root_[v2]), size_[v1] + size_[v2]);
    }

    // Construye una versión con todos los elementos de a en O(n)
    // (fusiones por parejas tipo cola, como en el build de un leftist heap).
    int build(const std::vector<T>& a) {
        if (a.empty()) return newVersion(0, 0);
        std::vector<int> q;
        q.reserve(a.size());
        for (const T& x : a) q.push_back(make(x, 0, 0));
        size_t head = 0;
        while (q.size() - head > 1) {
            int x = q[head++], y = q[head++];
            q.push_back(meld(x, y));
        }
        return newVersion(q[head], (int)a.size());
    }

    // ---- consultas (no crean versión) ----
    T top(int v) const {
        check(v);
        if (!root_[v]) throw std::runtime_error("top en heap vacio");
        return pool[root_[v]].key;
    }
    int  size(int v)  const { check(v); return size_[v]; }
    bool empty(int v) const { check(v); return size_[v] == 0; }
    int  versions()   const { return (int)root_.size(); }
    size_t nodes()    const { return pool.size(); }   // memoria usada (nº de nodos)

    // Devuelve los k mejores elementos de la versión v en orden, en O(k log k),
    // sin crear versiones (búsqueda tipo "best-first" sobre el árbol).
    std::vector<T> topK(int v, int k) const {
        check(v);
        std::vector<T> out;
        auto worse = [&](int a, int b) { return cmp(pool[b].key, pool[a].key); };
        std::vector<int> pq;                          // heap binario de índices
        if (root_[v]) pq.push_back(root_[v]);
        while (!pq.empty() && (int)out.size() < k) {
            std::pop_heap(pq.begin(), pq.end(), worse);
            int x = pq.back(); pq.pop_back();
            out.push_back(pool[x].key);
            for (int c : {pool[x].l, pool[x].r})
                if (c) { pq.push_back(c); std::push_heap(pq.begin(), pq.end(), worse); }
        }
        return out;
    }
};
