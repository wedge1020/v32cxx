# Diseño: `native Name;` — declaraciones de tipos opacos de paso directo

[Versión en inglés / in English](NATIVE_PASSTHROUGH.md) | [Versión en francés / en français](NATIVE_PASSTHROUGH.fr.md) <!-- langlinks -->

> **Estado (20260925-dev): implementado.** La gramática, el AST, el registro y
> cada rechazo semántico de abajo han aterrizado, con `tests/90sample.cpp` (usos
> legales) y `tests/91sample.cpp` (un error por cada forma rechazada). Donde la
> implementación se apartó del plan de más abajo, el plan se ha dejado tal como
> estaba y la diferencia se registra en [Tal como se construyó](#tal-como-se-construyó-20260925-dev)
> al final de este archivo.

## Problema

Las líneas `#include "X.h"` son texto de paso directo: v32c++ las reemite y nunca
las analiza. Consecuencia: todo tipo definido en una cabecera de C (date\_info,
time\_info, game\_signature) y toda función que lo toma o lo devuelve es
\*imposible de llamar desde el código fuente de v32c++\*, porque el nombre del
tipo no puede leerse como TYPE\_NAME — no está en la tabla de símbolos, así que
`date_info* out;` no se analiza como una declaración. time.hpp lo sorteó
reexpresando conversiones de pura matemática en clases; **memcard.h no puede
sortearse del mismo modo** (`card_signature_matches` es acceso al hardware —
`cmps R0` — no matemática). Esto se decidió de antemano en la ronda anterior;
esta es la especificación.

## Forma

```
native_decl:  NATIVE IDENTIFIER ';'   (and, later, qualified: NATIVE IDENTIFIER '{' ... — NOT now)
```

```cpp
native date_info;     // name becomes usable as a type in v32c++ source
native game_signature;

void test()
{
    date_info d;                  // NOT allowed (no layout known)
    date_info* p = 0;             // allowed: pointer to it
    translate_date( get_date(), p );   // the real C function, called
}
```

Semántica, con precisión:

- `native X;` inserta `X` en la tabla de símbolos como **SYM\_TYPEDEF** (el tipo
  exacto que inserta `typedef_decl`), en el ámbito actual, de modo que la
  clasificación TYPE\_NAME-frente-a-IDENTIFIER del analizador léxico lo recoge
  sin maquinaria nueva.
- **La generación de código no emite NADA** para la declaración en sí. El nombre
  se resuelve más adelante contra la cabecera de C real, exactamente como una
  llamada de paso directo.
- Un nuevo nodo del AST, `AST_NATIVE_DECL` (str1 = nombre). Existe en el árbol
  para que el volcado lo imprima y para que un futuro `native` con carga útil
  tenga dónde vivir; el recorrido del programa por la generación de código lo
  omite.

## La regla de solo puntero (el verdadero trabajo de sema)

El transpilador no sabe NADA sobre la disposición de un tipo nativo, así que el
uso por valor es siempre una mentira:

- **Permitido:** `X*`, `X&`, parámetros/retornos/variables locales/miembros `X*`,
  destinos de conversión `X*` (cuando las conversiones cualificadas se analicen).
- **Rechazado, de forma ruidosa:** variables locales por valor, parámetros,
  retornos, campos miembro, y `sizeof(X)`. Cada uno es un error de sema en el
  lugar de la declaración, p. ej. `semantic error: native type 'date_info' may only be used by pointer or reference (its layout is defined in a C header this transpiler does not parse)`.

Esto es deliberadamente más estricto que C, y es lo correcto: un tipo nativo por
valor produciría en silencio huecos de pila del tamaño equivocado.

## Plan de implementación (cuatro archivos, una ronda pequeña)

1. **lexer.l** — añadir el token de palabra clave `NATIVE` (el mismo patrón que
   STATIC/TYPEDEF: `g_last_ident_sym = NULL; return NATIVE;`). Reservar la
   palabra.
2. **parser.y** —
   - `%token NATIVE` en el bloque de tokens;
   - nueva regla de nivel superior:
     ```
     native_decl: NATIVE IDENTIFIER ';'
         {
             symtab_insert(g_symtab, g_symtab->current, $2, SYM_TYPEDEF);
             $$ = ast_new(AST_NATIVE_DECL, @1.first_line);
             $$->str1 = strdup($2);
         }
     ```
     añadida a la alternancia de nivel superior (en la misma posición que
     typedef\_decl). Precaución con GLR: es una regla aditiva y sin ambigüedad,
     pero regenera y comprueba `%expect` de todos modos; todo cambio de gramática
     recibe ese trato.
3. **ast.h/ast.c** — el tipo `AST_NATIVE_DECL` y el caso del volcado (una línea:
   `NativeDecl str1=%s`).
4. **sema.c** —
   - `collect_declarations` debería registrar los nombres nativos en el MISMO
     registro plano de typedefs que usan los typedefs, marcados como opacos
     (comprueba cómo almacena entradas ese registro; una nueva bandera
     `is_native` en la entrada, o un conjunto paralelo — ajústate a la forma ya
     existente del registro).
   - La comprobación de solo puntero: dondequiera que se resuelva el tipo de un
     var\_decl/parámetro/retorno, si el tipo base se resuelve a un nombre del
     registro de nativos Y el tipo no está envuelto en puntero/referencia, emitir
     el error ruidoso. Busca el punto central de resolución de tipos en lugar de
     comprobarlo en cada sitio — la lección de `infer_expr_type` de la ronda del
     registro de enumeraciones se aplica: un único cuello de botella vale más que
     búsquedas dispersas.
   - **La generación de código no emite nada**, pero `infer_expr_type` debería
     tratar `native X*` como un tipo puntero desconocido-pero-válido, no como un
     error.

## La única arruga que hay que comprobar antes de implementar

La entrada del registro de `typedef_decl` anota el tipo SUBYACENTE del typedef.
Un nativo no tiene ninguno. Comprueba qué hacen los consumidores con un tipo
subyacente NULL — si `print_type` o sema lo desreferencian, la entrada nativa
necesita un subyacente sintético (p. ej. ella misma, o un marcador de posición
que nunca se emita). Este es el único lugar donde el atajo de «reutilizar
SYM\_TYPEDEF» podría necesitar una decisión real; inspecciona primero los
consumidores de `semac/typereg` (el registro plano de typedefs).

## También decide (anótalo ahora, impleméntalo ahora): el piloto de memcard puede seguir

Una vez que `native` aterrice, v32/memcard.hpp se vuelve escribible:

```cpp
native game_signature;

namespace v32
{
    class CardScope { ... }   // save/restore pattern TBD
    bool signature_ok( game_signature* sig ) { return card_signature_matches( sig ); }
}
```

Pero el piloto de memcard es una ronda APARTE — no lo acoples a esta.

## Plan de pruebas: tests/90sample.cpp

- `native date_info;` + `native time_info;` a nivel superior, y luego llamar a
  las REALES `translate_date` / `translate_time` (la primera vez que se llama a
  los originales de C desde código fuente de v32c++) e imprimir los campos —
  comparación A/B directa con `v32::Date::set_from` sobre el mismo valor de
  `get_date()`; ambas deben imprimir el mismo A-M-D. Esto también demuestra la
  regresión de la conversión de clases frente a la propia API de C en la misma
  ejecución.
- Uso de punteros en las cuatro posiciones: variable local, parámetro, retorno,
  miembro.
- Los rechazos ruidosos, uno por línea (variable local por valor, parámetro por
  valor, retorno por valor, campo por valor, sizeof) — sema debe nombrar el tipo
  en cada error.
- Una compilación `--target=standard` con gcc del C generado (esta es la ronda
  en que gcc de escritorio SÍ puede verificar de extremo a extremo:
  date\_info/time\_info son estructuras de C corrientes y
  translate\_date/translate\_time son C puro).
- Deliberadamente NO se prueba: `native` de un nombre que no existe más adelante
  (es la misma clase que las llamadas de paso directo con errata — imposible de
  comprobar hasta que algún día exista el recurso de rastreo de v32pp).

## Fuera de alcance, dicho claramente

- `native` con etiquetas de enumeración/unión (`find_enum_or_union_decl` tiene su
  propia vía de emisión — tócala solo si un piloto realmente necesita una
  enumeración nativa).
- Nativo cualificado (`native v32::foo;`) — sin consumidor.
- El rastreo de estructuras/`#define` de v32pp — la alternativa para la OTRA
  mitad de la carencia del paso directo (la visibilidad de los `#define`), ronda
  aparte.

## Tal como se construyó (20260925-dev)

Lo que realmente se entregó, y en qué se diferencia del plan de arriba.

**Gramática.** `native_decl` **no** consume el `;` por sí misma — la alternativa
de nivel superior `native_decl ';'` lo hace, exactamente como `typedef_decl`. El
primer borrador tenía el `;` en ambos sitios, así que un simple `native X;` era
un error de sintaxis en lo que viniera a continuación y solo `native X;;` se
analizaba. Una segunda alternativa, `NATIVE TYPE_NAME`, hace idempotente una
declaración repetida: una vez que `X` está en la tabla de símbolos, el analizador
léxico devuelve `TYPE_NAME`, y varias cabeceras de `v32/` pueden declarar cada
una el mismo nativo. No hizo falta ningún cambio de `%expect` (sigue en 27).
Como `native_decl` es una `top_decl`, un nativo también puede declararse dentro
del cuerpo de un espacio de nombres; un uso cualificado (`v32::game_signature*`)
se imprime como el nombre simple de C, que es lo que define la cabecera posterior.

**Registro.** `TypedefRegEntry` ganó una bandera `is_native`, y
`typedef_registry_is_native(name)` (declarada en `sema.h`) es la única búsqueda
que usan todas las comprobaciones. Se conserva el tipo subyacente sintético con
su propio nombre (`n->type = ast_ident(name)`) para que nada que lea el nodo vea
NULL, **pero `find_typedef_target` nunca lo devuelve para un nativo.** Devolverlo
hacía que `resolve_typedef_chain` iterara X → X → X hasta que lo detenía su
protección de 64 pasos. Ahora la cadena se detiene en el nombre nativo en el
primer paso. La preocupación de la sección de la «arruga» queda resuelta así.

**Conflictos.** `native X;` donde `X` ya es una clase, estructura o typedef
ordinario es un error semántico (`'native X;' conflicts with an earlier declaration...`).
Marcar en silencio como nativo un tipo de disposición conocida prohibiría
retroactivamente todo uso suyo por valor.

**Dónde se comprueba la regla de solo puntero.** Todo está en sema.c, a través de
`check_native_pointer_only` (que primero pela el `const`, de modo que
`const date_info d` también se rechaza):

| Posición | Dónde |
| --- | --- |
| campo miembro de clase/estructura | recorrido de miembros de `compute_layout` |
| variable local | `check_node`, `AST_VAR_DECL` |
| variable global | `check_globals` |
| tipo de retorno + cada parámetro, prototipos y definiciones, funciones libres y métodos | `check_native_signature`, llamada una vez por función desde `check_function_body` / `access_check_free_functions` |
| `sizeof(X)` | `check_node`, `AST_SIZEOF` (`sizeof(X*)` pasa) |
| `p->field` / `r.field` a través de un nativo | `check_node`, `AST_MEMBER` (mediante `native_base_of`) |

Un borrador anterior también llamaba a `check_native_pointer_only` desde la
comprobación de tamaño de palabra de lower.c. Eso era un error de enlazado,
porque la función es estática de sema.c, y habría informado dos veces de cada
parámetro por valor. Esa llamada ha desaparecido. La advertencia de varias
palabras de lower.c es silenciosa para los nativos de todos modos: un nativo no
tiene `StructLayout`.

**El paso A/B del plan de pruebas todavía no puede escribirse.** El plan de
arriba llama a `translate_date` sobre un almacenamiento que posee el código
fuente de v32c++ y luego imprime los campos. Ambas mitades se rechazan ahora por
diseño: un `date_info d;` por valor no tiene disposición para reservar, y
`p->year` es acceso a un miembro a través de un nativo.
`translate_date(get_date(), p)` necesita un `p` que apunte a almacenamiento real,
y el código fuente de v32c++ no puede crear ese almacenamiento. Hoy el puntero
tiene que venir de C, por ejemplo una global del lado de C o una función de C que
devuelva `date_info*`. Dos caminos a seguir, ambos trabajo futuro:

1. **Nativos con tamaño:** `native date_info[3];` (o similar) da el número de
   palabras, lo que permite variables locales por valor y `sizeof` mientras el
   acceso a miembros sigue prohibido.
2. **Nativos con disposición:** `native date_info { int year; int month; int day; };`
   declara los campos y no emite nada. Esto es para lo que la nota sobre un
   «futuro `native` con carga útil» de la sección del AST reservaba espacio.

El piloto de memcard no tiene este problema, porque
`card_signature_matches(game_signature*)` solo necesita un puntero, y un puntero
a un `game_signature` definido del lado de C es suficiente.

**Los parámetros de referencia a un nativo** (`date_info&`) se aceptan y se bajan
a punteros como cualquier otra referencia. Tanto el reenvío `touch(d)` como
`touch(*p)` salen correctos. `&d` sobre un parámetro de referencia **no**: emite
`(&d)`, un `date_info **`. Eso no es específico de los nativos. Es la carencia
general de la bajada de referencias registrada en DESIGN_NOTES.md («Round:
`native` opaque types»), por lo que sample90 no usa esa escritura.

## El piloto de memcard (aterrizado)

`v32/memcard.hpp` es el primer consumidor real. Declara `native game_signature;`
a nivel de archivo y evita el problema del almacenamiento sin ninguna
característica nueva del lenguaje. Un `game_signature` *es* un `typedef int[20]`,
así que la API del envoltorio toma un simple `int*` a 20 palabras, que quien llama
puede declarar (`int sig[20] = "MYGAME";`), y lo convierte en la frontera con C:

```cpp
bool card_matches( int* signature )
{
    return card_signature_matches( (game_signature*)signature );
}
```

Verificado de extremo a extremo: el compilador real de Vircon32 acepta la
conversión, y `tests/94sample.cpp` se ejecuta en la consola emulada con una
tarjeta de memoria insertada. El archivo de la tarjeta contiene después la
firma en la palabra 0. Las opciones de nativos con tamaño o con disposición de
arriba siguen siendo la respuesta general para las estructuras de C cuyos campos
necesita leer el código del juego (`date_info`).
