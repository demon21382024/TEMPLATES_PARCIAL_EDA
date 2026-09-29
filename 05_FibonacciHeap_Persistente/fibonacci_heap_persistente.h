#pragma once
// ============================================================================
//  FIBONACCI HEAP PERSISTENTE  (totalmente persistente)
// ----------------------------------------------------------------------------
//  Problema: un Fibonacci heap tiene listas doblemente enlazadas y punteros al
//  padre => hay ciclos y el path copying directo no funciona (copiar un nodo
//  obligaría a copiar a todos los que lo apuntan).
//
//  Solución: el Fibonacci heap CLÁSICO (CLRS: marcas, link, consolidate, cut,
//  cascading cut) guardando sus nodos en una MEMORIA PERSISTENTE:
//
//   (1) Arreglo persistente  id -> registro  (árbol 4-ario implícito con path
//       copying, como un segment tree persistente). Los "punteros" parent,
//       child, left, right son IDs, así que un Handle (id) es válido en TODAS
//       las versiones que contienen ese nodo => decreaseKey / erase funcionan
//       sobre cualquier versión.
//   (2) Lista de raíces persistente aparte (celdas inmutables cons / concat):
//       insertar raíz y unir listas son O(1) y NO tocan los registros.
//
//  Versión = (raíz del arreglo, lista de raíces, id del mínimo, tamaño).
//
//  Optimización "transient": los nodos creados dentro de la operación en curso
//  (índice >= inicio de la operación) aún no pertenecen a ninguna versión, así
//  que se modifican en el lugar. Escribir 10 veces el mismo registro durante un
//  consolidate cuesta como escribirlo una vez.
//
//  Complejidades (L = profundidad del arreglo = log4(maxNodes), ~10):
//    insert       O(L)                     memoria O(L)
//    getMin       O(L)
//    extractMin   O(L · log n) amortizado  (*)
//    decreaseKey  O(L) amortizado          (*)
//    erase        O(L · log n) amortizado  (*)
//    merge        O(1) + unión de arreglos: O(L) si los ids de un heap son
//                 todos menores que los del otro; O(L·n) en el peor caso.
//                 Los heaps deben ser DISJUNTOS (no compartir nodos), como la
//                 unión del Fibonacci heap clásico. Si no, lanza excepción.
//  (*) Amortizado en uso "lineal" (cada versión se modifica una vez). Repetir
//      una operación cara sobre la MISMA versión vieja paga el costo cada vez:
//      la amortización no sobrevive a la persistencia (Okasaki). Es inherente
//      al Fibonacci heap.
//
//  Requisito: T con constructor por defecto.
//  Compare = std::less<T> -> min-heap ; std::greater<T> -> max-heap
//
//  Tipos de persistencia (modo del constructor):
//    Parcial    : operaciones solo sobre la última versión; merge prohibido.
//    Total      : operaciones sobre cualquier versión; merge prohibido.
//    Confluente : Total + merge(v1, v2) de heaps disjuntos.
//    Funcional  : la MEMORIA es funcional (un nodo publicado del arreglo
//                 persistente jamás se modifica); el algoritmo de encima es el
//                 imperativo de CLRS.
//  Para parcial con la técnica "fat node" (~2.7x menos memoria) ver
//  fibonacci_heap_parcial_fatnode.h
// ============================================================================
#include <vector>
#include <array>
#include <functional>
#include <stdexcept>
#include <utility>

#ifndef EDA_PERSISTENCIA
#define EDA_PERSISTENCIA
// Parcial    : se consultan todas las versiones, solo se modifica la última.
// Total      : se consulta y modifica cualquier versión (árbol de versiones).
// Confluente : Total + operaciones que combinan dos versiones (DAG de versiones).
enum class Persistencia { Parcial, Total, Confluente };
#endif

template <class T, class Compare = std::less<T>>
class PersistentFibonacciHeap {
public:
    using Handle = int;          // id del nodo; 0 = nulo

private:
    struct Rec {
        T   key;
        int parent, child;       // child = primer hijo
        int left, right;         // hermanos (lista doble terminada en 0)
        int degree;
        bool mark;
    };
    struct Version { int mem, roots, mn, n; };

    // ---- (1) arreglo persistente: árbol F-ario de 'levels' niveles ----
    static constexpr int BITS = 2, F = 1 << BITS, MASK = F - 1;
    int levels, CAP;
    std::vector<std::array<int, F>> tr;  // nodos internos (tr[0] = nulo)
    std::vector<Rec> recs;               // registros (recs[0] = nulo)
    size_t trStart = 1, recStart = 1;    // lo creado desde aquí es de la operación actual

    // ---- (2) lista de raíces persistente ----
    // celda {id, next} con id > 0  => cons(id, next)
    // celda {-a, b}                => concat(lista a, lista b)
    // lista 0 = vacía
    std::vector<std::array<int, 2>> rl;

    std::vector<Version> vers;
    int nextId = 1;
    Compare cmp;
    Persistencia modo;

    // ---- estado de trabajo de la operación en curso ----
    int cur = 0, roots = 0, mn = 0, n = 0;
    std::vector<int> buf, stk;           // buffers reutilizables

    // ======================= arreglo persistente =======================
    int recIdx(int mem, int id) const {
        int x = mem;
        for (int l = levels - 1; l > 0 && x; --l) x = tr[x][(id >> (l * BITS)) & MASK];
        return tr[x][id & MASK];         // si x == 0 -> 0 (registro nulo)
    }
    int own(int x) {                     // nodo modificable equivalente a x
        if ((size_t)x >= trStart) return x;
        std::array<int, F> c = tr[x];
        tr.push_back(c);
        return (int)tr.size() - 1;
    }
    // Copia el camino a la hoja de 'id'; devuelve el nodo del último nivel.
    int ownPath(int id) {
        int x = own(cur);
        cur = x;
        for (int l = levels - 1; l > 0; --l) {
            int s = (id >> (l * BITS)) & MASK;
            int c = own(tr[x][s]);
            tr[x][s] = c;
            x = c;
        }
        return x;
    }
    Rec rd(int id) const { return recs[recIdx(cur, id)]; }          // copia
    T key(int id) const { return recs[recIdx(cur, id)].key; }
    // Registro modificable. La referencia vale hasta el próximo wr()/eraseRec()
    // (pueden realocar): úsala dentro de una sola sentencia o bloque.
    Rec& wr(int id) {
        int x = ownPath(id);
        int r = tr[x][id & MASK];
        if ((size_t)r < recStart) {      // incluye r == 0 (registro nuevo)
            Rec c = recs[r];
            recs.push_back(c);
            r = (int)recs.size() - 1;
            tr[x][id & MASK] = r;
        }
        return recs[r];
    }
    void eraseRec(int id) { int x = ownPath(id); tr[x][id & MASK] = 0; }
    bool has(int mem, int id) const { return id > 0 && id < CAP && recIdx(mem, id) != 0; }

    // Une dos arreglos disjuntos (l = nivel del nodo, levels-1 = raíz)
    int unite(int a, int b, int l) {
        if (!a) return b;
        if (!b) return a;
        int x = own(0);
        for (int s = 0; s < F; ++s) {
            int ca = tr[a][s], cb = tr[b][s], c;
            if (l == 0) {
                if (ca && cb) throw std::invalid_argument("merge: los heaps comparten nodos");
                c = ca ? ca : cb;
            } else {
                c = unite(ca, cb, l - 1);
            }
            tr[x][s] = c;
        }
        return x;
    }

    // ======================= lista de raíces =======================
    int cons(int id, int list) { rl.push_back({id, list}); return (int)rl.size() - 1; }
    int concat(int a, int b) {
        if (!a) return b;
        if (!b) return a;
        rl.push_back({-a, b});
        return (int)rl.size() - 1;
    }
    template <class Fn> void forRoots(int list, Fn fn) {
        stk.clear();
        if (list) stk.push_back(list);
        while (!stk.empty()) {
            int c = stk.back(); stk.pop_back();
            while (c) {
                auto [a, b] = rl[c];
                if (a > 0) { fn(a); c = b; }
                else { stk.push_back(b); c = -a; }
            }
        }
    }

    // ======================= control de versiones =======================
    // empieza una operación que MODIFICA la versión v
    void begin(int v) {
        const Version& s = at(v);
        if (modo == Persistencia::Parcial && v != versions() - 1)
            throw std::logic_error("persistencia parcial: solo se modifica la ultima version");
        trStart = tr.size();
        recStart = recs.size();
        cur = s.mem; roots = s.roots; mn = s.mn; n = s.n;
    }
    int commit() {
        vers.push_back(Version{cur, roots, mn, n});
        return (int)vers.size() - 1;
    }
    const Version& at(int v) const {
        if (v < 0 || v >= (int)vers.size()) throw std::out_of_range("version invalida");
        return vers[v];
    }

    // ======================= Fibonacci heap (CLRS) =======================
    // y (raíz) pasa a ser el primer hijo de x (raíz)
    void link(int y, int x) {
        int c = rd(x).child;
        { Rec& w = wr(y); w.parent = x; w.left = 0; w.right = c; w.mark = false; }
        if (c) wr(c).left = y;
        Rec& wx = wr(x); wx.child = y; wx.degree++;
    }

    // Une árboles de igual grado hasta que todos los grados sean distintos y
    // reconstruye la lista de raíces y el mínimo.
    void consolidate(const std::vector<int>& cand) {
        int A[64] = {0};                 // grado máx. <= log_phi(n) < 64
        T   K[64];                       // clave de A[d] (evita relecturas)
        int maxd = -1;
        for (int w : cand) {
            int x = w;
            Rec xr = rd(x);
            T kx = xr.key;
            int d = xr.degree;
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
        roots = 0; mn = 0;
        int best = -1;
        for (int d = 0; d <= maxd; ++d) {
            int x = A[d];
            if (!x) continue;
            if (rd(x).parent) { Rec& w = wr(x); w.parent = 0; w.mark = false; }  // hijo promovido
            roots = cons(x, roots);
            if (best < 0 || cmp(K[d], K[best])) { best = d; mn = x; }
        }
    }

    // Elimina mn (estado de trabajo).
    void extractCore() {
        int z = mn;
        buf.clear();
        forRoots(roots, [&](int x) { if (x != z) buf.push_back(x); });
        for (int c = rd(z).child; c; c = rd(c).right) buf.push_back(c);
        eraseRec(z);
        --n;
        consolidate(buf);
    }

    // Corta x de su padre y y lo sube a la lista de raíces.
    void cut(int x, int y) {
        Rec xr = rd(x);
        if (xr.left) wr(xr.left).right = xr.right;
        else         wr(y).child = xr.right;
        if (xr.right) wr(xr.right).left = xr.left;
        wr(y).degree--;
        { Rec& w = wr(x); w.parent = 0; w.left = w.right = 0; w.mark = false; }
        roots = cons(x, roots);
    }
    void cascadingCut(int y) {
        while (true) {
            Rec yr = rd(y);
            int z = yr.parent;
            if (!z) return;
            if (!yr.mark) { wr(y).mark = true; return; }
            cut(y, z);
            y = z;
        }
    }

public:
    // maxNodes: máximo de inserciones TOTALES (sumando todas las versiones).
    explicit PersistentFibonacciHeap(int maxNodes = (1 << 20) - 1, Persistencia m = Persistencia::Confluente,
                                     Compare c = Compare()) : cmp(c), modo(m) {
        levels = 1;
        while ((1LL << (levels * BITS)) <= maxNodes) ++levels;   // ids 1..CAP-1
        CAP = 1 << (levels * BITS);
        tr.push_back({});                          // todo 0
        recs.push_back(Rec{T(), 0, 0, 0, 0, 0, false});
        rl.push_back({0, 0});
        vers.push_back(Version{0, 0, 0, 0});       // versión 0 = vacío
    }

    // ---- modificaciones: devuelven la NUEVA versión ----
    // Si h != nullptr, guarda el handle del nuevo nodo (sirve en versiones derivadas).
    int insert(int v, const T& k, Handle* h = nullptr) {
        begin(v);
        if (nextId >= CAP) throw std::length_error("capacidad agotada (sube maxNodes)");
        int id = nextId++;
        { Rec& w = wr(id); w = Rec{k, 0, 0, 0, 0, 0, false}; }
        roots = cons(id, roots);
        if (!mn || cmp(k, key(mn))) mn = id;
        ++n;
        if (h) *h = id;
        return commit();
    }

    int extractMin(int v) {
        begin(v);
        if (!mn) throw std::runtime_error("extractMin en heap vacio");
        extractCore();
        return commit();
    }

    // CONFLUENTE: la nueva versión tiene dos padres (v1 y v2).
    int merge(int v1, int v2) {
        if (modo != Persistencia::Confluente)
            throw std::logic_error("combinar versiones requiere persistencia confluente");
        const Version b = at(v2);        // copia: commit() puede realocar 'vers'
        begin(v1);
        if (!b.mn) return commit();
        if (!mn) { cur = b.mem; roots = b.roots; mn = b.mn; n = b.n; return commit(); }
        cur = unite(cur, b.mem, levels - 1);
        roots = concat(roots, b.roots);
        if (cmp(key(b.mn), key(mn))) mn = b.mn;
        n += b.n;
        return commit();
    }

    int decreaseKey(int v, Handle h, const T& k) {
        begin(v);
        if (!has(cur, h)) throw std::invalid_argument("decreaseKey: el nodo no esta en esta version");
        Rec x = rd(h);
        if (cmp(x.key, k)) throw std::invalid_argument("decreaseKey: la nueva clave es peor");
        wr(h).key = k;
        int y = x.parent;
        if (y && cmp(k, key(y))) { cut(h, y); cascadingCut(y); }
        if (cmp(k, key(mn))) mn = h;
        return commit();
    }

    int erase(int v, Handle h) {
        begin(v);
        if (!has(cur, h)) throw std::invalid_argument("erase: el nodo no esta en esta version");
        int y = rd(h).parent;
        if (y) { cut(h, y); cascadingCut(y); }
        mn = h;                          // equivale a decreaseKey(h, -inf) + extractMin
        extractCore();
        return commit();
    }

    // ---- consultas (no crean versión) ----
    T getMin(int v) const {
        const Version& s = at(v);
        if (!s.mn) throw std::runtime_error("getMin en heap vacio");
        return recs[recIdx(s.mem, s.mn)].key;
    }
    Handle minHandle(int v) const { return at(v).mn; }
    int  size(int v)  const { return at(v).n; }
    bool empty(int v) const { return at(v).n == 0; }
    bool contains(int v, Handle h) const { return has(at(v).mem, h); }
    T keyOf(int v, Handle h) const {
        const Version& s = at(v);
        if (!has(s.mem, h)) throw std::invalid_argument("keyOf: el nodo no esta en esta version");
        return recs[recIdx(s.mem, h)].key;
    }

    // Todas las claves de la versión v (O(n·L)); útil para depurar.
    std::vector<T> elements(int v) {
        const Version s = at(v);
        std::vector<T> out;
        std::vector<int> todo;
        forRoots(s.roots, [&](int x) { todo.push_back(x); });
        while (!todo.empty()) {
            int x = todo.back(); todo.pop_back();
            const Rec& r = recs[recIdx(s.mem, x)];
            out.push_back(r.key);
            for (int c = r.child; c; c = recs[recIdx(s.mem, c)].right) todo.push_back(c);
        }
        return out;
    }

    int versions() const { return (int)vers.size(); }
    int latest() const { return versions() - 1; }
    Persistencia mode() const { return modo; }
    // bytes usados por las estructuras internas (aprox.)
    size_t memoryBytes() const {
        return tr.size() * sizeof(tr[0]) + recs.size() * sizeof(Rec) + rl.size() * sizeof(rl[0])
             + vers.size() * sizeof(Version);
    }
};
