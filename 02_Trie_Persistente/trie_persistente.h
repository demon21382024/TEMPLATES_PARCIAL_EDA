#pragma once
// ============================================================================
//  TRIE PERSISTENTE  (path copying)
// ----------------------------------------------------------------------------
//  Cada inserción/borrado copia SOLO los nodos del camino raíz -> hoja
//  (L+1 nodos). El resto del trie se comparte con la versión anterior.
//
//  Incluye dos plantillas:
//
//  1) PersistentXorTrie<B>  (trie binario de B bits, multiconjunto de enteros)
//     insert/erase/count          O(B)
//     maxXor / minXor             O(B)
//     kth (k-ésimo menor)         O(B)
//     countLess (# elementos < x) O(B)
//     merge(v1, v2)               O(nodos donde ambos tries se solapan)
//     Todas las consultas aceptan un PAR de versiones (vl, vr) y trabajan sobre
//     "vr menos vl" (resta de contadores). Truco clásico: si la versión i es el
//     prefijo a[0..i-1], la consulta (l, r+1) responde sobre el subarreglo a[l..r].
//
//  2) PersistentStringTrie<SIGMA, BASE>  (multiconjunto de palabras)
//     insert/erase/count/countPrefix   O(|s|)
//     kth (k-ésima palabra lexicográfica) O(|s| * SIGMA)
//     merge(v1, v2)                    O(nodos solapados * SIGMA)
//     Memoria por inserción: (|s|+1) * (4*SIGMA + 8) bytes.
//
//  Nodo 0 = nulo (todos sus hijos apuntan a 0, contador 0) => la versión 0
//  (vacía) no cuesta memoria.
//
//  Tipos de persistencia (modo del constructor):
//    Parcial    : insert/erase solo sobre la última versión; merge prohibido.
//    Total      : insert/erase sobre cualquier versión; merge prohibido.
//    Confluente : Total + merge(v1, v2) = unión de multiconjuntos.
//    Funcional  : siempre (un nodo publicado jamás se modifica).
//  Consultar DOS versiones (vl, vr) está permitido en todos los modos: solo lee.
//  Para parcial con la técnica "fat node" ver trie_parcial_fatnode.h
// ============================================================================
#include <vector>
#include <array>
#include <string>
#include <cstdint>
#include <stdexcept>

#ifndef EDA_PERSISTENCIA
#define EDA_PERSISTENCIA
// Parcial    : se consultan todas las versiones, solo se modifica la última.
// Total      : se consulta y modifica cualquier versión (árbol de versiones).
// Confluente : Total + operaciones que combinan dos versiones (DAG de versiones).
enum class Persistencia { Parcial, Total, Confluente };
#endif

// ---------------------------------------------------------------------------
// 1) Trie binario persistente
// ---------------------------------------------------------------------------
template <int B = 30>
class PersistentXorTrie {
    static_assert(B >= 1 && B <= 63, "B entre 1 y 63");
public:
    using U = uint64_t;
private:
    std::vector<std::array<int, 2>> ch;   // hijos
    std::vector<int> cnt;                 // # de elementos en el subárbol
    std::vector<int> root_;
    Persistencia modo;

    int clone(int x) {
        std::array<int, 2> c = ch[x];
        int k = cnt[x];
        ch.push_back(c);
        cnt.push_back(k);
        return (int)ch.size() - 1;
    }
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
    // suma 'd' a todo el camino de x (d puede ser negativo) -> nueva versión
    int addPath(int v, U x, int d) {
        int old = root_[v];
        int nw = clone(old);
        cnt[nw] += d;
        int cur = nw;
        for (int b = B - 1; b >= 0; --b) {
            int bit = (x >> b) & 1;
            int nxt = clone(ch[old][bit]);
            cnt[nxt] += d;
            ch[cur][bit] = nxt;
            cur = nxt;
            old = ch[old][bit];
        }
        root_.push_back(nw);
        return (int)root_.size() - 1;
    }
    // unión de dos subárboles a profundidad 'lvl' (bits restantes); crea nodos
    // solo donde ambos existen, lo demás se comparte
    int unite(int a, int b, int lvl) {
        if (!a) return b;
        if (!b) return a;
        int x = clone(a);
        cnt[x] = cnt[a] + cnt[b];
        if (lvl == 0) return x;
        for (int bit = 0; bit < 2; ++bit) {
            int c = unite(ch[a][bit], ch[b][bit], lvl - 1);
            ch[x][bit] = c;
        }
        return x;
    }
    int c(int a, int b) const { return cnt[a] - cnt[b]; }   // contador de "a - b"

public:
    explicit PersistentXorTrie(Persistencia m = Persistencia::Confluente, size_t reserveNodes = 0) : modo(m) {
        ch.reserve(reserveNodes + 1);
        cnt.reserve(reserveNodes + 1);
        ch.push_back({0, 0});
        cnt.push_back(0);
        root_.push_back(0);                  // versión 0 = vacío
    }

    // ---- modificaciones: devuelven la nueva versión ----
    int insert(int v, U x, int times = 1) { checkWrite(v); return addPath(v, x, times); }
    int erase(int v, U x, int times = 1) {
        checkWrite(v);
        if (count(v, x) < times) throw std::runtime_error("erase: no hay suficientes copias");
        return addPath(v, x, -times);
    }
    // CONFLUENTE: nueva versión = multiconjunto v1 ∪ v2 (se suman los contadores)
    int merge(int v1, int v2) {
        checkConfluent(); check(v1); check(v2);
        int r = unite(root_[v1], root_[v2], B);
        root_.push_back(r);
        return (int)root_.size() - 1;
    }

    // ---- consultas sobre la versión v, o sobre (vr - vl) ----
    int size(int v) const { check(v); return cnt[root_[v]]; }

    int count(int v, U x) const {
        check(v);
        int a = root_[v];
        for (int b = B - 1; b >= 0 && a; --b) a = ch[a][(x >> b) & 1];
        return cnt[a];
    }

    // max (x XOR y) con y en (vr - vl). Requiere que haya al menos un elemento.
    U maxXor(int vl, int vr, U x) const {
        check(vl); check(vr);
        int a = root_[vr], z = root_[vl];
        if (c(a, z) <= 0) throw std::runtime_error("maxXor: conjunto vacio");
        U res = 0;
        for (int b = B - 1; b >= 0; --b) {
            int want = ((x >> b) & 1) ^ 1;            // bit opuesto
            if (c(ch[a][want], ch[z][want]) > 0) { res |= U(1) << b; a = ch[a][want]; z = ch[z][want]; }
            else { a = ch[a][want ^ 1]; z = ch[z][want ^ 1]; }
        }
        return res;
    }
    U maxXor(int v, U x) const { return maxXor(0, v, x); }

    U minXor(int vl, int vr, U x) const {
        check(vl); check(vr);
        int a = root_[vr], z = root_[vl];
        if (c(a, z) <= 0) throw std::runtime_error("minXor: conjunto vacio");
        U res = 0;
        for (int b = B - 1; b >= 0; --b) {
            int want = (x >> b) & 1;                  // mismo bit
            if (c(ch[a][want], ch[z][want]) > 0) { a = ch[a][want]; z = ch[z][want]; }
            else { res |= U(1) << b; a = ch[a][want ^ 1]; z = ch[z][want ^ 1]; }
        }
        return res;
    }
    U minXor(int v, U x) const { return minXor(0, v, x); }

    // k-ésimo menor (k desde 1) en (vr - vl)
    U kth(int vl, int vr, int k) const {
        check(vl); check(vr);
        int a = root_[vr], z = root_[vl];
        if (k < 1 || k > c(a, z)) throw std::out_of_range("kth fuera de rango");
        U res = 0;
        for (int b = B - 1; b >= 0; --b) {
            int left = c(ch[a][0], ch[z][0]);
            if (k <= left) { a = ch[a][0]; z = ch[z][0]; }
            else { k -= left; res |= U(1) << b; a = ch[a][1]; z = ch[z][1]; }
        }
        return res;
    }
    U kth(int v, int k) const { return kth(0, v, k); }

    // # de elementos < x en (vr - vl)
    int countLess(int vl, int vr, U x) const {
        check(vl); check(vr);
        int a = root_[vr], z = root_[vl], res = 0;
        for (int b = B - 1; b >= 0 && c(a, z) > 0; --b) {
            int bit = (x >> b) & 1;
            if (bit) res += c(ch[a][0], ch[z][0]);
            a = ch[a][bit]; z = ch[z][bit];
        }
        return res;
    }
    int countLess(int v, U x) const { return countLess(0, v, x); }

    int versions() const { return (int)root_.size(); }
    int latest() const { return versions() - 1; }
    Persistencia mode() const { return modo; }
    size_t nodes() const { return ch.size(); }
};

// ---------------------------------------------------------------------------
// 2) Trie de cadenas persistente
// ---------------------------------------------------------------------------
template <int SIGMA = 26, char BASE = 'a'>
class PersistentStringTrie {
    struct Node {
        int ch[SIGMA];
        int pass;   // # de palabras que pasan por este nodo
        int end;    // # de palabras que terminan aquí
    };
    std::vector<Node> t;
    std::vector<int> root_;
    Persistencia modo;

    int clone(int x) { Node n = t[x]; t.push_back(n); return (int)t.size() - 1; }
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
    static int id(char c) {
        int k = (unsigned char)c - (unsigned char)BASE;
        if (k < 0 || k >= SIGMA) throw std::invalid_argument("caracter fuera del alfabeto");
        return k;
    }
    int addPath(int v, const std::string& s, int d) {
        int old = root_[v];
        int nw = clone(old);
        t[nw].pass += d;
        int cur = nw;
        for (char chr : s) {
            int k = id(chr);
            int nxt = clone(t[old].ch[k]);
            t[nxt].pass += d;
            t[cur].ch[k] = nxt;
            cur = nxt;
            old = t[old].ch[k];
        }
        t[cur].end += d;
        root_.push_back(nw);
        return (int)root_.size() - 1;
    }
    int walk(int v, const std::string& s) const {
        int a = root_[v];
        for (char chr : s) { if (!a) return 0; a = t[a].ch[id(chr)]; }
        return a;
    }
    // unión de dos subárboles (recursión de profundidad = palabra más larga)
    int unite(int a, int b) {
        if (!a) return b;
        if (!b) return a;
        int x = clone(a);
        t[x].pass = t[a].pass + t[b].pass;
        t[x].end = t[a].end + t[b].end;
        for (int c = 0; c < SIGMA; ++c) {
            int y = unite(t[a].ch[c], t[b].ch[c]);
            t[x].ch[c] = y;
        }
        return x;
    }

public:
    explicit PersistentStringTrie(Persistencia m = Persistencia::Confluente, size_t reserveNodes = 0) : modo(m) {
        t.reserve(reserveNodes + 1);
        t.push_back(Node{});      // nulo: todo en 0
        root_.push_back(0);
    }

    int insert(int v, const std::string& s) { checkWrite(v); return addPath(v, s, +1); }
    int erase(int v, const std::string& s) {
        checkWrite(v);
        if (count(v, s) == 0) throw std::runtime_error("erase: la palabra no existe");
        return addPath(v, s, -1);
    }
    // CONFLUENTE: nueva versión = multiconjunto de palabras v1 ∪ v2
    int merge(int v1, int v2) {
        checkConfluent(); check(v1); check(v2);
        int r = unite(root_[v1], root_[v2]);
        root_.push_back(r);
        return (int)root_.size() - 1;
    }

    int count(int v, const std::string& s) const { check(v); return t[walk(v, s)].end; }
    int countPrefix(int v, const std::string& p) const { check(v); return t[walk(v, p)].pass; }
    int size(int v) const { check(v); return t[root_[v]].pass; }

    // k-ésima palabra en orden lexicográfico (k desde 1, con repeticiones)
    std::string kth(int v, int k) const {
        check(v);
        int a = root_[v];
        if (k < 1 || k > t[a].pass) throw std::out_of_range("kth fuera de rango");
        std::string res;
        while (true) {
            if (k <= t[a].end) return res;     // la palabra termina aquí
            k -= t[a].end;
            for (int c = 0; c < SIGMA; ++c) {
                int b = t[a].ch[c];
                if (k <= t[b].pass) { res.push_back(char(BASE + c)); a = b; break; }
                k -= t[b].pass;
            }
        }
    }

    int versions() const { return (int)root_.size(); }
    int latest() const { return versions() - 1; }
    Persistencia mode() const { return modo; }
    size_t nodes() const { return t.size(); }
};
