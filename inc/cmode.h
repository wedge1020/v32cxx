#ifndef CMODE_H
#define CMODE_H

#include "ast.h"

/*
 * C input (g_c_mode, driver.h): the AST passes that run right after the
 * parse, before anything else looks at the tree. See cmode.c.
 */

/* Every node the parser built from `struct X` / `union X` / `enum X`, and
 * every struct/union/enum declaration that has a tag. Filled by parser.y
 * for C input; read by cmode_separate_tags. */
extern AstList g_tag_refs;
extern AstList g_tag_decls;

void cmode_separate_tags(AstList *decls);
void cmode_unify_prototypes(AstList *decls);
void cmode_drop_unused_args(AstList *decls);
void cmode_lower_main_params(AstList *decls);
/* Returns the number of errors it reported. */
int  cmode_rewrite_variadics(AstList *decls);

#endif /* CMODE_H */
