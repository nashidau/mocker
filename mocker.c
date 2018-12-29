
#include <stdio.h>

#include "mocker.h"

static int get_stream_id(const char *name)
{
	int i;
	for (i=0; i<input_stream_nr; i++) {
		if (strcmp(name, stream_name(i))==0)
			return i;
	}
	return -1;
}

static char *determine_symbol_name(struct symbol *sym)
{
	static char *stars = "**********";
	static char buf[100];
	const char *name = "";
	const char *type = "FIXME";
	int ptrs = 0;

	while (sym->ctype.base_type->type == SYM_PTR) {
		ptrs ++;
		sym = sym->ctype.base_type;
	}

	if (sym->ident) {
		name = show_ident(sym->ident);
	}

	type = builtin_typename(sym);
	
	snprintf(buf,sizeof(buf), "%s %.*s%s", type, ptrs, stars, name);

	return buf;
}

const char *
get_typename(struct ctype *ctype) {
	int pcount = 0;
	static char buf[200] = "?";
	char *p;
	const char *builtin;
	p = buf;

	while (ctype->base_type->type == SYM_PTR) {
		pcount ++;
		ctype = &ctype->base_type->ctype;
	}
	
	builtin = builtin_typename(ctype->base_type);
	if (builtin) {
		strcpy(p, builtin);
		while (*p) p ++;
	} else if (ctype->base_type->type == SYM_STRUCT) {
		strcpy(p, "struct ");
		p += 7;
		strcpy(p, ctype->base_type->ident->name);
		p += ctype->base_type->ident->len;
	} else {
		*p = '?';
		p ++;
	}
	if (pcount) 
		*p ++ = ' ';
	while (pcount --) {
		*p = '*';
		p ++;
	}
	*p = 0;

	return buf;
}

/*
 * Print the mock information for a single symbol (a member of a struct).
 * Only function pointers are displayed, all others are skipped
 */
static void mock_symbol(struct symbol *sym)
{
	struct symbol *arg;
	struct symbol *fn;
	const char *leader = "";

	if (!sym) return;

	// Not a pointer -> Not a function pointer ;-)
	if (!is_ptr_type(sym)) {
		return;
	}
	// Do we have a funtion behind that pointer
	// FIXME: wrap this in a helper 
	fn = sym->ctype.base_type->ctype.base_type;
	if (fn->type != SYM_FN) {
		return;
	}
	//printf("Found function member: %s\n", show_ident(sym->ident));

	//printf("\treturn: %d %s\n", fn->ctype.base_type->type, builtin_typename(fn->ctype.base_type));


	// So we have a function pointer:
	printf("Function pointer: Name: %s;  Returns %s/%s\n", show_ident(sym->ident),
						get_typename(&fn->ctype),
						builtin_typename(fn->ctype.base_type));


	FOR_EACH_PTR(fn->arguments, arg) {
		printf("\tArg: %s %s\n", get_typename(&arg->ctype), arg->ident ? show_ident(arg->ident) : "(anon)");
	} END_FOR_EACH_PTR(arg);
}

/* Walk the members of a struct */
static void mock_members(struct symbol_list *list)
{
	struct symbol *sym;

	FOR_EACH_PTR(list, sym) {
		mock_symbol(sym);
	} END_FOR_EACH_PTR(sym);
}


static void examine_symbol(struct symbol *sym)
{
	if (!sym) return;
	// Not sure what this does; from c2xml
	if (sym->ident && sym->ident->reserved) return;
	if (sym->type == SYM_ENUM) return;
	if (sym->type != SYM_STRUCT) return;

	printf("// File: %s %d\n", stream_name(sym->pos.stream), sym->pos.line);
	//printf("Type name is %s\n", get_type_name(sym->type));
	//printf("Identifier: %s\n", show_ident(sym->ident));

//	if (strcmp(show_ident(sym->ident), "foo")) {
//		printf("Not the one\n");
//		return;
//	}

	
	//printf("Sym type is %d (struct: %d)\n", sym->type, SYM_STRUCT);
	printf("struct %s {\n", sym->ident->name);

	mock_members(sym->symbol_list);
	printf("}\n");

#if 0
	const char *base;
	int array_size;

	if (!sym)
		return;
	if (sym->aux)		/*already visited */
		return;

	if (sym->ident && sym->ident->reserved)
		return;

	child = new_sym_node(sym, get_type_name(sym->type), node);
	examine_modifiers(sym, child);
	examine_layout(sym, child);

	if (sym->ctype.base_type) {
		if ((base = builtin_typename(sym->ctype.base_type)) == NULL) {
			if (!sym->ctype.base_type->aux) {
				examine_symbol(sym->ctype.base_type, root_node);
			}
			xmlNewProp(child, BAD_CAST "base-type",
			           xmlGetProp((xmlNodePtr)sym->ctype.base_type->aux, BAD_CAST "id"));
		} else {
			newProp(child, "base-type-builtin", base);
		}
	}
	if (sym->array_size) {
		/* TODO: modify get_expression_value to give error return */
		array_size = get_expression_value(sym->array_size);
		newNumProp(child, "array-size", array_size);
	}


	switch (sym->type) {
	case SYM_STRUCT:
	case SYM_UNION:
		examine_members(sym->symbol_list, child);
		break;
	case SYM_FN:
		examine_members(sym->arguments, child);
		break;
	case SYM_UNINITIALIZED:
		newProp(child, "base-type-builtin", builtin_typename(sym));
		break;
	default:
		break;
	}
	return;
#endif
}



static void examine_namespace(struct symbol *sym)
{
	if (sym->ident && sym->ident->reserved)
		return;

	switch(sym->namespace) {
	case NS_STRUCT:
		examine_symbol(sym);
		break;
	case NS_MACRO:
	case NS_TYPEDEF:
	case NS_SYMBOL:
	case NS_NONE:
	case NS_LABEL:
	case NS_ITERATOR:
	case NS_UNDEF:
	case NS_PREPROCESSOR:
	case NS_KEYWORD:
		break;
	default:
		die("Unrecognised namespace type %d",sym->namespace);
	}

}


static void examine_symbol_list(const char *file, struct symbol_list *list)
{
	struct symbol *sym;
	int stream_id = get_stream_id (file);

	if (!list)
		return;
	FOR_EACH_PTR(list, sym) {
		if (sym->pos.stream == stream_id)
			examine_namespace(sym);
	} END_FOR_EACH_PTR(sym);
}


int main(int argc, char **argv) {
	struct string_list *filelist = NULL;
	struct symbol_list *symlist = NULL;
	char *file;

	printf("Mocker starting\n");

	symlist = sparse_initialize(argc, argv, &filelist);

	FOR_EACH_PTR_NOTAG(filelist, file) {
		examine_symbol_list(file, symlist);
		sparse_keep_tokens(file);
		examine_symbol_list(file, file_scope->symbols);
		examine_symbol_list(file, global_scope->symbols);
	} END_FOR_EACH_PTR_NOTAG(file);

	return 0;
}
