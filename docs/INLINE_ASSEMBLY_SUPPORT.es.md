# Soporte de ensamblador en línea para v32c++

[Versión en inglés / in English](INLINE_ASSEMBLY_SUPPORT.md) | [Versión en francés / en français](INLINE_ASSEMBLY_SUPPORT.fr.md) <!-- langlinks -->

## Objetivo

Permitir que el código fuente de v32c++ contenga ensamblador en línea,
pasándolo tal cual al compilador de C de Vircon32 subyacente (que lo soporta de
forma nativa — véase el `video.h` de la biblioteca estándar, donde cada función
de la GPU es un envoltorio delgado de `asm { ... }`). Aceptar también las demás
formas habituales de escribir ensamblador en línea que se ven por ahí (el
`asm("...")` básico de GCC/Clang, `__asm`/`__asm__`, `asm volatile`), siempre
que, **al apuntar a Vircon32, la salida sea siempre la forma con llaves del C de
Vircon32**.

## Qué formatos existen, y qué hacemos con cada uno

| Forma en el código fuente | Ejemplo | Tratamiento |
| --- | --- | --- |
| Nativa de Vircon32 | `asm { "out GPU_Command, GPUCommand_DrawRegion" }` | Paso directo literal (la interpolación `{param}` dentro de los literales de cadena la resuelve el compilador de C de Vircon32, no nosotros) |
| `asm` básico de GCC/Clang | `asm("nop");`, `asm ("a" "b");` | Aceptado; reemitido en la forma con llaves de Vircon32 con `--target=vircon32` (el valor por defecto), mantenido entre paréntesis con `--target=standard` |
| Variantes de escritura | `__asm`, `__asm__`, `asm volatile`, `__volatile__` | Todas se analizan léxicamente como el mismo token; el calificador se acepta y se ignora (solo afecta a la optimización, que no hacemos) |
| `asm` extendido de GCC | `asm("..." : "=r"(x) : "r"(y) : "cc")` | **Rechazado con un diagnóstico específico** — las restricciones de operandos no tienen equivalente en Vircon32; el sustituto previsto es la interpolación `{param}` del dialecto de Vircon32 |
| `__asm { mov eax, 1 }` de MSVC (instrucciones sin comillas) | — | Rechazado: el cuerpo no es un literal de cadena, no hay nada que traducir. La forma con llaves solo se acepta cuando cada instrucción es un literal entre comillas, es decir, exactamente la forma de Vircon32 |

Solo se soporta el ensamblador **a nivel de sentencia**. Eso basta para todo el
patrón de la biblioteca estándar (`int f() { asm { "in R0, X" } }` — el bloque
asm deja su resultado en R0 y el compilador de Vircon32 lo trata como el valor de
retorno). Las *expresiones* asm al estilo GCC (`int x = asm(...);`) no existen en
el asm básico de todos modos y se dejan sin soporte.

## Diseño

Un nodo nuevo del AST, un token nuevo, un conjunto nuevo de reglas de gramática,
un caso nuevo de generación de código. Sin cambios en sema.c ni en lower.c: una
sentencia asm no tiene hijos que reescribir — ni `this`, ni referencias, ni
operadores — y cada recorrido de bajada existente usa `default: break;`, así que
un tipo de sentencia desconocido ya pasa sin tocarse. (De todos modos conviene
añadir AST\_ASM a sus `switch` como un caso sin efecto con un comentario, según el
propio estilo del código de documentar las omisiones deliberadas.)

El cuerpo del asm se guarda como una **AstList de nodos AST\_STRING\_LIT, uno por
cada literal de cadena consecutivo en el orden del código fuente** — no una
única cadena concatenada — para que la generación de código pueda imprimir cada
literal en su propia línea, que es como el propio `video.h` da formato a los
bloques de varias instrucciones y mantiene legible el C generado. La regla
STRING\_LITERAL del analizador léxico ya quita las comillas y conserva los
escapes como pares de barra invertida en bruto, y la regla de recomillado
existente de la generación de código lo recorre de ida y vuelta exactamente (el
mismo razonamiento ya documentado en el caso de impresión de AST\_STRING\_LIT), de
modo que las llaves al estilo `{texture_id}` y cualquier escape sobreviven
literalmente.

`ival` en el nodo registra el *dialecto tal como se escribió* (`0` = forma con
llaves, `1` = forma entre paréntesis/GCC), lo cual solo importa en el modo
`--target=standard`; en el modo Vircon32 ambos dialectos emiten la forma con
llaves.

---

## Parche 1 — `lexer.l`

Añadir al bloque de palabras clave (junto a `"goto"`, `"sizeof"`, etc.):

```c
/* Inline assembly. All three spellings collapse to one token; the
 * optional volatile qualifier is handled in the grammar. Not reserved
 * as an identifier anywhere else, matching real C/C++ where asm is a
 * (nonstandard-but-universal) extension keyword rather than core. */
"asm"        { g_last_ident_sym = NULL; return ASM; }
"__asm"      { g_last_ident_sym = NULL; return ASM; }
"__asm__"    { g_last_ident_sym = NULL; return ASM; }
"volatile"         { g_last_ident_sym = NULL; return VOLATILE; }
"__volatile__"     { g_last_ident_sym = NULL; return VOLATILE; }
```

Nota: `volatile` actualmente no es una palabra clave en absoluto (el README lo
lista como no soportado). Convertirla en una únicamente para que `asm volatile`
se analice es seguro — antes se leía como un IDENTIFIER, pero ningún programa
real de este subconjunto podría haberla usado como nombre de variable sin haber
sido ya señalado. Si prefieres no reservarla globalmente, una alternativa es una
regla léxica dedicada `asm[ \t]+volatile` que se compare *antes* de la tabla de
palabras clave; el enfoque de la palabra clave es más sencillo y coincide con
cómo la trata GCC.

## Parche 2 — `parser.y`

**Declaraciones** (junto con los demás `%token`; `%glr-parser` y `%locations` ya
están activados, no hace falta nada nuevo ahí):

```bison
%token ASM
%token VOLATILE
```

**Nuevo no terminal** (colocar cerca de `arg_list`):

```bison
/* Consecutive string literals forming one asm body -- `asm { "a" "b" }`
 * and asm("a" "b") both allow implicit concatenation, same as real C's
 * own adjacent-literal rule. One AST_STRING_LIT per literal, in source
 * order; codegen re-quotes and prints one per line. The lexer already
 * stripped the quotes and left escapes raw, so a `{param}` inside the
 * literal (Vircon32's own operand-interpolation syntax, see video.h)
 * survives this round trip verbatim -- we never interpret the body. */
asm_string_list:
      STRING_LITERAL           { $$ = ast_list_new(); ast_list_append(&$$, $1); }
    | asm_string_list STRING_LITERAL  { $$ = $1; ast_list_append(&$$, $2); }
    ;
```

(Comprueba la firma de `ast_list_append` — si toma un `AstList*` en lugar de
devolver el nodo, guarda los literales en un nodo envoltorio con forma de
`AST_INIT_LIST` del mismo modo que lo hace `opt_member_init_list` y pon ese
envoltorio en el campo `list` del AST\_ASM. Sirve cualquiera de las dos formas;
elige la que encaje con las reglas de alrededor.)

**Reglas de sentencia** (dentro de `stmt:`):

```bison
    | ASM '{' asm_string_list '}'
        {
            /* Vircon32 C's own native form -- pure pass-through. */
            $$ = ast_new(AST_ASM, @1.first_line);
            $$->list = $3;
            $$->ival = 0;  /* written in brace form */
        }
    | ASM '(' asm_string_list ')' ';'
        {
            /* GCC/Clang basic asm. No operands allowed (basic asm has
             * none); codegen re-emits this as Vircon32 brace form when
             * targeting Vircon32, or keeps the parenthesized spelling
             * in --target=standard. An extended asm (with ':'
             * constraint sections) does NOT parse here -- the ':' after
             * the string list is a syntax error at this rule; see the
             * targeted diagnostic note below. */
            $$ = ast_new(AST_ASM, @1.first_line);
            $$->list = $3;
            $$->ival = 1;  /* written in GCC parenthesized form */
        }
    | VOLATILE ASM '(' asm_string_list ')' ';'
        {
            /* `asm volatile("...")` -- the qualifier only governs
             * optimization/reordering, which this transpiler performs
             * neither of, so it is accepted and dropped. */
            $$ = ast_new(AST_ASM, @2.first_line);
            $$->list = $4;
            $$->ival = 1;
        }
    | ASM VOLATILE '(' asm_string_list ')' ';'
        {
            /* __volatile__ spelled after the keyword (GCC documents
             * both orders historically; harmless to accept). */
            $$ = ast_new(AST_ASM, @1.first_line);
            $$->list = $4;
            $$->ival = 1;
        }
```

**Diagnóstico específico para el asm extendido.** En lugar de dejar que
`asm("..." : "=r"(x) : ...)` muera en GLR con un genérico «syntax error,
unexpected ':'», añade una regla que reconozca la forma de las restricciones y
lo informe con claridad:

```bison
    | ASM '(' asm_string_list ':' /* deliberately incomplete */
        {
            yyerror("extended asm with operand constraints is not "
                    "supported: Vircon32 C uses '{param}' interpolation "
                    "inside the literal instead (see video.h); write the "
                    "operands directly in the instruction text");
            YYERROR;
        }
```

(Añade más alternativas con `:` si quieres que el mensaje salte también con 2–3
secciones separadas por dos puntos; con `%define parse.error verbose`, la regla
única de arriba más un comentario en DESIGN\_NOTES probablemente basta — el
primer `:` es lo que aparece siempre.)

Nota sobre la forma con llaves: `ASM '{' ... '}'` no introduce ninguna ambigüedad
de gramática de la que preocuparse — `asm` es un token dedicado, así que el
analizador nunca lo confunde con una sentencia de expresión, y no se produce
ninguna división GLR más allá de lo que el archivo ya maneja.

## Parche 3 — `ast.h`

Añadir a `AstKind` (región de sentencias, cerca de AST\_GOTO/AST\_LABEL) con el
estilo de comentarios de documentación del código:

```c
    AST_ASM,              /* list=AST_STRING_LIT per string literal of
                              the body, in source order; str1 unused.
                              ival=0 when written in Vircon32 brace form
                              (`asm { "..." }`), 1 when written in GCC
                              basic-asm parenthesized form (`asm("...")`)
                              -- which only changes what --target=standard
                              mode prints, since Vircon32 output is ALWAYS
                              the brace form (the Vircon32 C compiler is
                              the one that understands it). Pure
                              pass-through: nothing in sema.c or lower.c
                              interprets the body, and the `{param}`
                              interpolation inside each literal (see
                              video.h's own GPU wrappers) is resolved by
                              the Vircon32 C compiler, not by us. The
                              lexer already stripped each literal's quotes
                              and left escapes raw; codegen.c's existing
                              re-quote rule (see AST_STRING_LIT's own
                              print case) round-trips them exactly. A leaf
                              statement -- never an expression, matching
                              basic asm in real compilers; the standard
                              library's own `int f() { asm { "in R0, X" } }`
                              pattern works as-is because Vircon32 C
                              treats the block's R0 as the return value. */
```

Actualiza también el `switch` de `ast_dump` en `ast.c` si enumera los tipos
(añade un caso que imprima el número de literales).

## Parche 4 — `codegen.c`

En el `switch` de `print_stmt` (el mismo `switch` cuyo `default:` imprime hoy
«unhandled statement kind» — este es exactamente el hueco que salta hoy con
cualquier sentencia asm):

```c
        case AST_ASM:
            /* Pure pass-through: the Vircon32 C compiler is the one that
             * actually assembles this. TARGET_VIRCON32 always emits the
             * brace form regardless of which dialect was written, since
             * that's the only form Vircon32's compiler accepts; TARGET_
             * STANDARD keeps the dialect as written (gcc/clang accept the
             * parenthesized basic-asm spelling natively, and a brace-form
             * body re-wrapped in parentheses is equally valid there).
             * One string literal per line, matching how video.h itself
             * formats multi-instruction blocks -- purely cosmetic.
             * Re-quoting each literal is the same round-trip rule as
             * AST_STRING_LIT's own case above: the lexer stored the inner
             * text with escapes still raw, so wrapping it back in quotes
             * reproduces the source literal exactly, `{param}` braces
             * included. */
            if (s->ival == 1 && g_target == TARGET_STANDARD) {
                indent_spaces(out, indent);
                fprintf(out, "asm(");
                for (int i = 0; i < s->list.count; i++) {
                    if (i > 0) fprintf(out, " ");
                    fprintf(out, "\"%s\"", s->list.items[i]->str1);
                }
                fprintf(out, ");\n");
            }
            else {
                indent_spaces(out, indent);
                fprintf(out, "asm\n");
                indent_spaces(out, indent);
                fprintf(out, "{\n");
                for (int i = 0; i < s->list.count; i++) {
                    indent_spaces(out, indent + 1);
                    fprintf(out, "\"%s\"\n", s->list.items[i]->str1);
                }
                indent_spaces(out, indent);
                fprintf(out, "}\n");
            }
            break;
```

(Usa el nombre real de la global del modo de destino del archivo — los resultados
de la búsqueda muestran que ambos modos de `--target` existen en codegen.c; si se
escribe de otro modo, p. ej. una enumeración `g_target_mode`, sustitúyelo. El
auxiliar `explain()` también puede recibir una línea para el modo detallado, p.
ej. «inline assembly passed through verbatim; Vircon32's C compiler assembles
it».)

## Parche 5 — `lower.c` / `sema.c` (sin efecto, pero explícito)

Ninguna de las dos pasadas necesita tocar AST\_ASM — no hay `this`, ni referencias,
ni operadores dentro del cuerpo de un literal de cadena — y ambas ya hacen
`default: break;` ante tipos de sentencia desconocidos. Según la propia
convención del código de documentar las no-acciones deliberadas, añade a cada
`switch` que recorra sentencias:

```c
        case AST_ASM:
            /* Nothing to do: an asm body is opaque string literals --
             * no `this`, member references, or references to rewrite.
             * Passed through to codegen.c verbatim. */
            break;
```

Una salvedad real que merece una entrada en DESIGN\_NOTES: un `goto` que salta
*por encima* de una sentencia asm, o una sentencia asm colocada entre la
declaración de una variable local y la llamada a su destructor al salir del
ámbito, se comporta como cualquier otra sentencia opaca — la bajada ya emite las
llamadas a destructores alrededor de los límites de las sentencias, y el asm no
lo perturba. Pero un asm que modifique registros que el compilador cree vivos a
través de él es problema del programador, exactamente como en C real.

---

## Comprobación de ida y vuelta con `video.h`

Cada patrón de la cabecera subida se transpila sin cambios:

```cpp
void select_texture( int texture_id )
{
    asm
    {
        "mov R0, {texture_id}"
        "out GPU_SelectedTexture, R0"
    }
}
```

- léxico: `asm` → ASM; los dos STRING\_LITERAL conservan en bruto el texto
  interior `"mov R0, {texture_id}"` (las llaves son caracteres de cadena
  corrientes para flex).
- sintáctico: salta la regla con llaves, dos AST\_STRING\_LIT en `list`,
  `ival = 0`.
- sema/lower: sin efecto.
- generación de código: imprime el bloque con llaves idéntico, un literal por
  línea.

La forma que devuelve un valor también funciona:

```cpp
int get_selected_texture()
{
    asm
    {
        "in R0, GPU_SelectedTexture"
    }
}
```

El asm a nivel de sentencia deja el resultado en R0, y el C de Vircon32 lo trata
como el valor de retorno de la función — no hace falta una forma de expresión.

Y un archivo de usuario en dialecto GCC:

```cpp
void nop() { asm("nop"); }
```

produce, en modo Vircon32:

```c
void nop( void )
{
    asm
    {
        "nop"
    }
}
```

## Pruebas sugeridas

1. Forma con llaves con uno y con varios literales (copia dos funciones tal cual
   de `video.h`, p. ej. `select_texture` y `get_drawing_point` — esta última
   también ejercita el modismo de salida por puntero `"push R1"` /
   `"mov [R1], R0"`).
2. `asm` básico de GCC, con y sin `volatile`, con las tres escrituras de la
   palabra clave.
3. Interpolación `{param}` con un parámetro, una variable local y (en un método)
   un campo con `this` inyectado — confirma que la inyección de `this` NO
   reescribe dentro de los literales (no debe: `{this->x}` no es sintaxis válida
   de asm de Vircon32; quien llama debería extraerlo antes a una variable local).
4. `asm` extendido → se espera el diagnóstico específico, no un error de sintaxis
   genérico.
5. Ida y vuelta con `--target=standard` de ambos dialectos a través de gcc.
6. asm dentro de un método (tras la inyección de `this`) y dentro del cuerpo de un
   bucle — confirma que los recorridos de sentencias lo dejan pasar.

## En uso: `v32/v32io.hpp` (20261009-dev)

La cabecera de teclado/ratón es la primera cabecera de `v32/` que depende del
ensamblador en línea, dentro de un método. `v32::IoDevice` escribe una rutina de
código máquina de 68 palabras en un arreglo miembro en su constructor y la llama
desde `scan()`:

```cpp
entry = (int)(&routine[ 0 ]);
asm
{
    "PUSH  R0"
    "MOV   R0, {entry}"
    "CALL  R0"
    "POP   R0"
}
```

`{entry}` es una variable local del método, de modo que la interpolación
`{param}` del compilador de Vircon32 la resuelve (un miembro, `{this->...}`, no
sería válido; de ahí la variable local). Comprobado de extremo a extremo en la
consola sin pantalla con `tests/119sample.cpp`. Ver `docs/V32IO.es.md`.
