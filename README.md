# Estructuras de Datos Persistentes — plantillas C++17

Cada carpeta tiene dos headers reutilizables y un `main.cpp` con ejemplos, una demo de los 4 tipos
de persistencia y pruebas aleatorias contra fuerza bruta.

| Carpeta | Path copying (parcial / total / confluente / funcional) | Fat node (parcial) |
|---|---|---|
| `01_Heap_Persistente` | `heap_persistente.h` — leftist heap | `heap_parcial_fatnode.h` — heap binario en arreglo |
| `02_Trie_Persistente` | `trie_persistente.h` — trie XOR y trie de cadenas | `trie_parcial_fatnode.h` — tries con contadores con historial |
| `03_SegmentTree_Persistente` | `segtree_persistente.h` — puntual (monoide) y suma en rango | `segtree_parcial_fatnode.h` — segment tree en arreglo |
| `04_Stack_Persistente` | `stack_persistente.h` — lista inmutable + jump pointers | `stack_parcial_fatnode.h` — pila en arreglo |
| `05_FibonacciHeap_Persistente` | `fibonacci_heap_persistente.h` — CLRS sobre memoria persistente | `fibonacci_heap_parcial_fatnode.h` — CLRS con nodos con historial |

```bash
cd 01_Heap_Persistente
g++ -std=c++17 -O2 main.cpp -o heap && ./heap
```

## Los 4 tipos de persistencia

| Tipo | Qué se puede hacer | Forma de las versiones | Cómo se consigue aquí |
|---|---|---|---|
| **Parcial** | Consultar cualquier versión; modificar **solo la última** | Línea `0 → 1 → 2 → …` | `Persistencia::Parcial` en el constructor, o las clases `Partial*` (fat node) |
| **Total** | Consultar **y modificar** cualquier versión | Árbol | `Persistencia::Total` |
| **Confluente** | Total + **combinar dos versiones** en una nueva | DAG (una versión con dos padres) | `Persistencia::Confluente` (valor por defecto) + `merge` / `concat` |
| **Funcional** | Ningún nodo se modifica jamás: solo se crean nodos nuevos | — (es una forma de implementar) | Todas las clases con path copying lo son; las de fat node **no** |

### Modo en las clases con path copying

```cpp
PersistentHeap<int> h(Persistencia::Parcial);   // Parcial | Total | Confluente
int v1 = h.push(0, 5);
int v2 = h.push(v1, 3);
h.push(v1, 7);       // Parcial: lanza std::logic_error (v1 no es la última)
h.merge(v1, v2);     // Parcial o Total: lanza std::logic_error (merge es confluente)
```

- Consultar dos versiones a la vez (por ejemplo `kth(vl, vr, k)` en el trie o el segment tree) está
  permitido en todos los modos, porque solo lee y no crea una versión.
- El modo solo agrega verificaciones: el costo de las operaciones es el mismo.

### Operación confluente de cada estructura

| Estructura | Operación | Resultado | Costo |
|---|---|---|---|
| Heap | `merge(v1, v2)` | unión de multiconjuntos, incluso `merge(v, v)` | O(log n) |
| Trie XOR / cadenas | `merge(v1, v2)` | unión de multiconjuntos (se suman contadores) | O(nodos donde ambos se solapan) |
| Segment tree puntual | `merge(v1, v2)` | `a[i] = op(a1[i], a2[i])` | O(nodos solapados) |
| Segment tree suma en rango | `merge(v1, v2)` | `a[i] = a1[i] + a2[i]` | O(nodos solapados) |
| Stack | `concat(v1, v2)` | pila v2 encima de v1 | O(\|v2\|); v1 se comparte |
| Fibonacci heap | `merge(v1, v2)` | unión; los heaps deben ser **disjuntos** | O(1) + unión de memorias |

### Path copying vs fat node (para persistencia parcial)

| | Path copying | Fat node (Driscoll, Sarnak, Sleator, Tarjan 1989) |
|---|---|---|
| Idea | Copiar los nodos del camino modificado | Cada campo o celda guarda su historial `[(versión, valor)]` |
| Modificar | Crea nodos nuevos | Agrega una entrada (o pisa la de la versión actual) |
| Leer la versión v | O(1) por nodo | Búsqueda binaria: O(log m) por nodo (O(1) en la última versión) |
| Tipos | Parcial, total, confluente | Solo parcial |
| ¿Funcional? | Sí | No (los nodos cambian) |
| Sirve para estructuras de arreglo o con ciclos | Mal: copiar un arreglo cuesta O(n) y los ciclos obligan a copiar todo | Sí: heap binario, pila y segment tree en arreglo, Fibonacci heap CLRS |

Las 5 clases de fat node comparten `FatArray<T>`, un arreglo parcialmente persistente. Está
copiado en cada header dentro del mismo `#ifndef EDA_FAT_ARRAY_H`, así que cada carpeta es
independiente y se pueden incluir varias juntas.

```cpp
PartialPersistentHeap<int> h;    // sin número de versión al modificar: siempre la última
h.push(5); h.push(3); h.pop();   // versiones 1, 2, 3
h.top(2);                        // consulta cualquier versión -> 3
```

## Complejidades

| Estructura | Operación | Path copying | Fat node (parcial) |
|---|---|---|---|
| Heap | push, pop | O(log n) | O(log n) |
| | top(v) | O(1) | O(log m) |
| | merge | O(log n) | — |
| Trie | insert, erase | O(B) o O(\|s\|) | igual |
| | consultas en v | O(B) o O(\|s\|) | × O(log m) |
| Segment tree | update | O(log n) | O(log n) |
| | query(v) | O(log n) | O(log n · log m) |
| Stack | push, pop, top, size | O(1) | O(1); top(v) en O(log m) |
| | kth(v, k) | O(log n) (jump pointers) | O(log m) (acceso directo) |
| Fibonacci heap | insert, getMin | O(L) | O(1) |
| | decreaseKey | O(L) amort. | O(1) amort. |
| | extractMin, erase | O(L · log n) amort. | O(log n) amort. |

`m` es el número de modificaciones de una celda. `L ≈ log₄(maxNodes)`, que vale unos 10.

## Convenciones comunes

- **Versiones con enteros**: la versión `0` es la estructura vacía (o el arreglo inicial). Cada
  operación que modifica **devuelve el id de la versión nueva**.
- **Memoria**: los nodos viven en un `std::vector` contiguo y los punteros son índices `int`
  (4 bytes en lugar de 8, mejor uso de caché, sin `new`/`delete`). El nodo `0` es el **nulo**.
  Los constructores aceptan un tamaño de reserva para evitar realocaciones.

## Notas para el examen

- **Heap**: con path copying se usa el leftist heap, porque copiar el arreglo de un heap binario
  cuesta O(n); la espina derecha mide O(log n), así que `merge` es barato. El *skew heap* no sirve:
  es amortizado y la amortización se rompe con la persistencia. Con **fat node**, en cambio, el
  heap binario de arreglo sí funciona: cada movimiento del sift-up/down agrega una entrada.
- **Trie**: con la versión `i` como el prefijo `a[0..i-1]`, la resta de contadores
  `cnt[raíz_{r+1}] - cnt[raíz_l]` responde consultas sobre el subarreglo `a[l..r]`. En la versión
  fat node los punteros a hijos se escriben una sola vez, así que solo los contadores necesitan
  historial.
- **Segment tree**: el nodo nulo (`0`) representa un subárbol "todo identidad", así que un árbol
  lleno de ceros no ocupa memoria. Las actualizaciones en rango usan *lazy permanente*: la marca
  se queda en el nodo y la consulta la acumula al bajar. Si se propagara, cada consulta tendría
  que crear nodos o escribir historial.
- **Stack**: una versión es solo el puntero al tope. Los *jump pointers* de Myers (en binario
  sesgado) dan acceso al k-ésimo elemento en O(log n) con O(1) memoria extra por nodo. En la
  versión fat node `pop` no escribe nada: solo cambia el tamaño guardado por versión.
- **Fibonacci heap**: tiene ciclos (listas dobles y punteros al padre), así que el path copying
  directo no funciona.
  - **Total/confluente**: el algoritmo clásico (marcas, `link`, `consolidate`, `cut`,
    `cascading cut`) sobre una **memoria persistente**, un arreglo persistente `id → registro`
    donde los punteros son ids. Los *handles* valen en todas las versiones, así que
    `decreaseKey` y `erase` funcionan en cualquier versión.
  - **Parcial (fat node)**: el mismo CLRS, pero cada nodo guarda su historial. Sobre la última
    versión corre tan rápido como el Fibonacci heap normal.
  - Las cotas amortizadas valen en **uso lineal**. Repetir una operación cara sobre la misma
    versión vieja paga el costo cada vez; esto es inherente a cualquier estructura amortizada
    persistente (Okasaki).
  - Memoria medida con 100k `insert` + 100k `extractMin`: **366 MB** con path copying y
    **138 MB** con fat node.
