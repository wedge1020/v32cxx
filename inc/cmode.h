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
extern AstList g_struct_refs;
extern AstList g_c_fwd_tags;   /* tags used but never defined: need `struct X;` */

void cmode_separate_tags(AstList *decls);
void cmode_unify_prototypes(AstList *decls);
/* The return type of a function that was declared but never defined (its
 * prototype is dropped from the output), or NULL: lets type inference see
 * through `struct P *me(void);` when only the prototype is in the file. */
AstNode *cmode_dropped_prototype_type(const char *name);
void cmode_drop_unused_args(AstList *decls);
void cmode_lower_main_params(AstList *decls);
/* Calls through `...` (C and C++): each call's extra arguments become an
 * array of words, passed as the variadic function's last parameter. Runs
 * after sema (which says which calls these are) and before lowering.
 * Returns the number of errors reported. */
int  rewrite_variadic_calls(AstList *decls);

#endif /* CMODE_H */
