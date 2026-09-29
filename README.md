# Estructuras de Datos Persistentes — plantillas C++17

Cada carpeta tiene un header reutilizable (`*.h`) y un `main.cpp` con un ejemplo de uso y una
prueba aleatoria contra fuerza bruta.

```bash
cd 01_Heap_Persistente
g++ -std=c++17 -O2 main.cpp -o heap && ./heap
```

## Convenciones comunes

- **Versiones con enteros**: la versión `0` es la estructura vacía (o el arreglo inicial). Cada
  operación que modifica **devuelve el id de una versión nueva** y no toca las anteriores. Puedes
  modificar **cualquier** versión, no solo la última (persistencia total).
- **Memoria**: los nodos viven en un `std::vector` contiguo y los punteros son índices `int`
  (4 bytes en lugar de 8, mejor uso de caché, sin `new`/`delete`). El nodo `0` es el **nulo**.
  Los constructores aceptan `reserveNodes` para evitar realocaciones.
- **Path copying**: una modificación copia solo los nodos del camino afectado; el resto se
  comparte entre versiones.

## Resumen

| Carpeta | Estructura | Operaciones | Tiempo | Memoria nueva por operación |
|---|---|---|---|---|
| `01_Heap_Persistente` | Leftist heap | push, pop, merge | O(log n) | O(log n) |
| | | top | O(1) | — |
| | | build | O(n) | O(n) |
| `02_Trie_Persistente` | Trie binario (XOR) | insert, erase, maxXor, minXor, kth, countLess (también sobre subarreglo `[l, r]`) | O(B) | O(B) |
| | Trie de cadenas | insert, erase, count, countPrefix | O(\|s\|) | O(\|s\|·Σ) |
| | | kth lexicográfico | O(\|s\|·Σ) | — |
| `03_SegmentTree_Persistente` | Seg. tree puntual (monoide genérico: suma, mín, máx…) | set, apply | O(log n) | O(log n) |
| | | query | O(log n) | — |
| | | kth en subarreglo | O(log n) | — |
| | Seg. tree con suma en rango (lazy permanente) | add en rango | O(log n) | O(log n) |
| | | suma de rango | O(log n) | — |
| `04_Stack_Persistente` | Pila (lista inmutable + jump pointers) | push, pop, top, size | O(1) | O(1) |
| | | k-ésimo desde el tope, fondo común | O(log n) | — |
| `05_FibonacciHeap_Persistente` | Fibonacci heap (CLRS) sobre memoria persistente | insert, getMin | O(L) | O(L) |
| | | decreaseKey | O(L) amort. | O(L) amort. |
| | | extractMin, erase | O(L·log n) amort. | O(L·log n) amort. |
| | | merge | O(1) + unión de memorias | — |

`L ≈ log₄(maxNodes)`, que vale unos 10.

## Notas para el examen

- **Heap**: no se usa el heap binario de arreglo porque copiar el arreglo cuesta O(n). El leftist
  heap garantiza una espina derecha de O(log n), así que `merge` hace path copying barato.
  (El *skew heap* no sirve: es amortizado, y la amortización se rompe con la persistencia.)
- **Trie**: con la versión `i` como el prefijo `a[0..i-1]`, la resta de contadores
  `cnt[raíz_{r+1}] - cnt[raíz_l]` responde consultas sobre el subarreglo `a[l..r]`.
- **Segment tree**: el nodo nulo (`0`) representa un subárbol "todo identidad", así que un árbol
  lleno de ceros no ocupa memoria. Para las actualizaciones en rango se usa *lazy permanente*:
  la marca se queda en el nodo y la consulta la acumula al bajar. Si se propagara, cada consulta
  tendría que crear nodos.
- **Stack**: una versión es solo el puntero al tope. Los *jump pointers* de Myers (en binario
  sesgado) dan acceso al k-ésimo elemento en O(log n) con O(1) memoria extra por nodo.
- **Fibonacci heap**: tiene ciclos (listas dobles y punteros al padre), así que el path copying
  directo no funciona. Se implementa el algoritmo clásico (marcas, `link`, `consolidate`, `cut`,
  `cascading cut`) sobre una **memoria persistente**: un arreglo persistente `id → registro`
  donde los punteros son ids. Los *handles* (`ids`) valen en todas las versiones, así que
  `decreaseKey` y `erase` funcionan en cualquier versión. La lista de raíces es una lista
  persistente aparte (`cons` / `concat` en O(1)).
  - Las cotas amortizadas valen en **uso lineal**. Repetir una operación cara sobre la misma
    versión vieja paga el costo cada vez; esto es inherente a cualquier estructura amortizada
    persistente (Okasaki).
  - `merge` requiere heaps **disjuntos**; si comparten nodos, lanza `invalid_argument`.
  - Memoria real medida: ~180 B por `insert` y ~3,5 KB por `extractMin` en uso lineal
    (100k `insert` + 100k `extractMin` ≈ 370 MB).
