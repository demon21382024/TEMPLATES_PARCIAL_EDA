#pragma once
// ============================================================================
//  STACK PERSISTENTE  (lista enlazada inmutable con nodos compartidos)
// ----------------------------------------------------------------------------
//  Una versión es simplemente un puntero al nodo tope. push crea UN nodo que
//  apunta al tope anterior; pop devuelve el puntero al siguiente. Nunca se
//  modifica un nodo => todas las versiones forman un árbol que comparte colas.
//
//  Complejidades:
//    push / pop / top / size : O(1) tiempo, push usa O(1) memoria, pop 0 memoria
//    kth(v, k)               : O(log n)   (k-ésimo desde el tope)
//    commonBottom(v1, v2)    : O(log n)   (parte común más larga del fondo)
//    concat(v1, v2)          : O(|v2|)    (v2 encima de v1; v1 se comparte)
//
//  kth y commonBottom usan "jump pointers" en binario sesgado (Myers 1983):
//  cada nodo guarda UN salto extra (O(1) memoria en vez de O(log n) del binary
//  lifting) y aun así cualquier ancestro se alcanza en O(log n) saltos.
//
//  Nodo 0 = fondo (pila vacía), profundidad 0.
//
//  Tipos de persistencia (modo del constructor):
//    Parcial    : push/pop solo sobre la última versión; concat prohibido.
//    Total      : push/pop sobre cualquier versión; concat prohibido.
//    Confluente : Total + concat(v1, v2).
//    Funcional  : siempre (un nodo publicado jamás se modifica).
//  Para parcial con la técnica "fat node" ver stack_parcial_fatnode.h
// ============================================================================
#include <vector>
#include <stdexcept>
#include <algorithm>

#ifndef EDA_PERSISTENCIA
#define EDA_PERSISTENCIA
// Parcial    : se consultan todas las versiones, solo se modifica la última.
// Total      : se consulta y modifica cualquier versión (árbol de versiones).
// Confluente : Total + operaciones que combinan dos versiones (DAG de versiones).
enum class Persistencia { Parcial, Total, Confluente };
#endif

template <class T>
class PersistentStack {
    struct Node {
        T   val;
        int next;    // nodo de abajo
        int jump;    // salto (binario sesgado)
        int depth;   // = tamaño de la pila cuyo tope es este nodo
    };
    std::vector<Node> t;
    std::vector<int> top_;        // nodo tope de cada versión
    Persistencia modo;

    int newVersion(int x) { top_.push_back(x); return (int)top_.size() - 1; }
    void check(int v) const {
        if (v < 0 || v >= (int)top_.size()) throw std::out_of_range("version invalida");
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
    // crea un nodo con valor x encima del nodo p y devuelve su índice
    int pushNode(int p, const T& x) {
        int j = t[p].jump, jj = t[j].jump;
        int jump = (t[p].depth - t[j].depth == t[j].depth - t[jj].depth) ? jj : p;
        t.push_back(Node{x, p, jump, t[p].depth + 1});
        return (int)t.size() - 1;
    }
    // ancestro de x con profundidad d (d <= depth(x))
    int ancestor(int x, int d) const {
        while (t[x].depth > d) x = (t[t[x].jump].depth >= d) ? t[x].jump : t[x].next;
        return x;
    }

public:
    explicit PersistentStack(Persistencia m = Persistencia::Confluente, size_t reserveNodes = 0) : modo(m) {
        t.reserve(reserveNodes + 1);
        t.push_back(Node{T(), 0, 0, 0});
        newVersion(0);                         // versión 0 = pila vacía
    }

    // ---- modificaciones: devuelven la nueva versión ----
    int push(int v, const T& x) {
        checkWrite(v);
        return newVersion(pushNode(top_[v], x));
    }
    int pop(int v) {
        checkWrite(v);
        if (!top_[v]) throw std::runtime_error("pop en pila vacia");
        return newVersion(t[top_[v]].next);
    }
    // CONFLUENTE: pila v2 puesta encima de la pila v1 (el tope queda el de v2).
    // Los nodos de v1 se comparten; los de v2 se copian: O(|v2|).
    int concat(int v1, int v2) {
        checkConfluent(); check(v1); check(v2);
        std::vector<int> nodes;                // nodos de v2 del tope al fondo
        for (int x = top_[v2]; x; x = t[x].next) nodes.push_back(x);
        int p = top_[v1];
        for (int i = (int)nodes.size() - 1; i >= 0; --i) {
            T val = t[nodes[i]].val;           // copia: pushNode puede realocar
            p = pushNode(p, val);
        }
        return newVersion(p);
    }

    // ---- consultas ----
    T top(int v) const {
        check(v);
        if (!top_[v]) throw std::runtime_error("top en pila vacia");
        return t[top_[v]].val;
    }
    int  size(int v)  const { check(v); return t[top_[v]].depth; }
    bool empty(int v) const { return size(v) == 0; }

    // k-ésimo desde el tope (k = 0 es el tope)
    T kth(int v, int k) const {
        check(v);
        int x = top_[v];
        if (k < 0 || k >= t[x].depth) throw std::out_of_range("kth fuera de rango");
        return t[ancestor(x, t[x].depth - k)].val;
    }

    // Tamaño del fondo común (mismos nodos compartidos) de dos versiones.
    int commonBottom(int v1, int v2) const {
        check(v1); check(v2);
        int a = top_[v1], b = top_[v2];
        if (t[a].depth > t[b].depth) std::swap(a, b);
        b = ancestor(b, t[a].depth);
        // búsqueda binaria por saltos: subir ambos mientras sean distintos
        while (a != b) {
            if (t[a].jump != t[b].jump) { a = t[a].jump; b = t[b].jump; }
            else { a = t[a].next; b = t[b].next; }
        }
        return t[a].depth;
    }

    // Elementos de la versión v, del tope al fondo  (O(n))
    std::vector<T> toVector(int v) const {
        check(v);
        std::vector<T> out;
        for (int x = top_[v]; x; x = t[x].next) out.push_back(t[x].val);
        return out;
    }

    int versions() const { return (int)top_.size(); }
    int latest() const { return versions() - 1; }
    Persistencia mode() const { return modo; }
    size_t nodes() const { return t.size(); }
};
