#ifndef GENERIC_H
#define GENERIC_H

#include "ast.h"

/*
 * Transpiler-level generics: `std::array<T, N>` and `std::vector<T>`
 * without templates.
 *
 * This project does not do templates. What it does instead is what
 * cfront-era C++ did with <generic.h>: for each distinct `array<T, N>`
 * the program names, write out one ORDINARY class with T and N filled
 * in, and let the rest of the pipeline (sema, lowering, codegen) treat
 * it like any class a student could have typed by hand.
 *
 *   std::array<Enemy, 8>   ->   class array_Enemy_8 { Enemy m_data[8]; ... };
 *
 * How it fits together:
 *   - prescan.c consumes `#include <array>` (it must never reach the
 *     generated C) and sets g_generic_array_enabled;
 *   - lexer.l returns STD_ARRAY for `std::array` (or a bare `array`
 *     before '<' once `using namespace std;` was seen);
 *   - parser.y's type_spec calls generic_array_type(), which returns
 *     the instantiated class's NAME as a plain type and queues the
 *     instantiation;
 *   - main.c calls generic_instantiate_pending() after the main parse:
 *     the queued classes are generated as C++ source (generic.c holds
 *     the text), parsed by the same parser, and spliced into the
 *     program just ahead of the first declaration that uses each one.
 */

extern int g_generic_array_enabled;  /* `#include <array>` was seen */
extern int g_generic_vector_enabled; /* `#include <vector>` was seen */
extern int g_using_std;             /* `using namespace std;` was seen */

/* `elem` is the element type as parsed; the length is either a literal
 * (`len_name` NULL) or a named constant (`len_name` non-NULL -- an enum
 * constant or `const int`, printed by name). Returns the type node to
 * use in place of `array<T, N>`, or NULL after reporting an error. */
AstNode *generic_array_type(AstNode *elem, int len_value, const char *len_name, int line);

/* The same for `std::vector<T>`. */
AstNode *generic_vector_type(AstNode *elem, int line);

/* Parses and splices every queued instantiation into g_program.
 * Returns 0 on success, nonzero if the generated source failed to
 * parse (an internal error, already reported). */
int generic_instantiate_pending(void);

#endif /* GENERIC_H */
