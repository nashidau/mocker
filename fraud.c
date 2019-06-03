/**
 * Generate a cmocka style mock from a template file.
 *
 * Usage:
 *  fraud -t template.fraud -o mock.c header.h <sparse options>
 *
 *   --include Include.path
 *
 * Templates should contain the following lines
 * **INCLUDES** (at the start of the line).
 *
 */

#include <assert.h>
#include <getopt.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Sparse
#include "expression.h"
#include "parse.h"
#include "scope.h"
#include "symbol.h"


struct include {
	struct include *next;
	const char *include;
};


static void examine_symbol_list(const char *file, struct symbol_list *list, FILE *outfp);
static void examine_namespace(struct symbol *sym, FILE *outfp);
static void mock_symbol(struct symbol *sym, FILE *outfp);
static const char *get_typename(struct ctype *ctype);
static void dump_mocks(struct string_list *filelist, struct symbol_list *symlist, FILE *outfp);

static int get_stream_id(const char *name)
{
	int i;
	for (i=0; i<input_stream_nr; i++) {
		if (strcmp(name, stream_name(i))==0)
			return i;
	}
	return -1;
}

static struct include *
push_include(struct include *list, const char *include) {
	struct include *added;
	if (!include) {
		return list;
	}
	added = malloc(sizeof(struct include));
	assert(added != NULL);
	added->include = strdup(include);
	added->next = list;
	return added;
}

static void
dump_includes(struct include *list, FILE *fp) {
	for ( ; list; list = list->next) {
		fprintf(fp, "#include <%s>\n", list->include);
	}
}

int
main(int argc, char **argv) {
	struct string_list *filelist = NULL;
	struct symbol_list *symlist = NULL;
	char opt;
	struct include *includes = NULL;
	const char *optstring = "t:o:i";
	const struct option options[] = {
		{ "template", required_argument, NULL, 't' },
		{ "output", required_argument, NULL, 'o' },
		{ "include", required_argument, NULL, 'i' },
		{ NULL, 0, 0, 0 },
	};
	const char *template = NULL;
	const char *output = NULL;
	char buf[BUFSIZ];
	FILE *fp, *outfp;

	while ((opt = getopt_long(argc, argv, optstring, options, NULL)) != -1) {
		switch (opt) {
		case 't':
			template = optarg;
			break;
		case 'o':
			output = optarg;
			break;
		case 'i':
			includes = push_include(includes, optarg);
			break;
		default:
			printf("Unkown Arg: %c\n", opt);
			exit(1);
		}
	}

	if (template == NULL || output == NULL) {
		printf("Need template & output\n");
		exit(1);
	}

	//	- open each of the files
	//	- Process the header file
	// 	- Walk through template
	symlist = sparse_initialize(argc + 1 - optind, argv + optind - 1, &filelist);	

	fp = fopen(template, "r");
	assert(fp);

	outfp = fopen(output, "w");
	assert(outfp);

	while (fgets(buf, sizeof(buf), fp)) {
		if (strncmp("** INCLUDES", buf, 10) == 0) {
			dump_includes(includes, outfp);
		} else if (strncmp("** MOCKS", buf, 7) == 0) {
			dump_mocks(filelist, symlist, outfp);
		} else {
			fputs(buf, outfp);
		}
	}
	
	exit(0);
}

static void
dump_mocks(struct string_list *filelist, struct symbol_list *symlist, FILE *outfp) {
	char *file;

	FOR_EACH_PTR_NOTAG(filelist, file) {
		examine_symbol_list(file, symlist, outfp);
		sparse_keep_tokens(file);
		examine_symbol_list(file, file_scope->symbols, outfp);
		examine_symbol_list(file, global_scope->symbols, outfp);
	} END_FOR_EACH_PTR_NOTAG(file);

}

static void examine_symbol_list(const char *file, struct symbol_list *list, FILE *outfp)
{
	struct symbol *sym;
	int stream_id = get_stream_id(file);

	if (!list)
		return;
	FOR_EACH_PTR(list, sym) {
		if (sym->pos.stream == stream_id)
			examine_namespace(sym, outfp);
	} END_FOR_EACH_PTR(sym);
}

static void examine_namespace(struct symbol *sym, FILE *outfp)
{
	if (sym->ident) {
		if (sym->type == SYM_NODE)
			mock_symbol(sym, outfp);
		return;
	}
}

static void mock_symbol(struct symbol *sym, FILE *outfp)
{
	struct symbol *arg;
	struct symbol *fn;
	bool first = true;

	if (!sym) return;

	fn = sym->ctype.base_type;
	
	if (fn->type != SYM_FN) {
		return;
	}

	// So we have a function pointer:
	fprintf(outfp, "%s\n%s(\n", get_typename(&fn->ctype), show_ident(sym->ident));

	FOR_EACH_PTR(fn->arguments, arg) {
		fprintf(outfp, "%s\t\t%s %s", first ? "": ",\n", get_typename(&arg->ctype), show_ident(arg->ident));
		first = false;
	} END_FOR_EACH_PTR(arg);
	if (first) {
		fprintf(outfp, "\t\tvoid");
	}
	fprintf(outfp, ") {\n");

	FOR_EACH_PTR(fn->arguments, arg) {
		if (strcmp(show_ident(arg->ident), "result") == 0) {
			fprintf(outfp, "\tmemcpy(%s, mock_ptr_type, sizeof(*%s));\n",
					show_ident(arg->ident), show_ident(arg->ident));
		} else if (arg->ctype.base_type->type == SYM_PTR) {
			fprintf(outfp, "\tcheck_expected_ptr(%s);\n", show_ident(arg->ident));
		} else {
			fprintf(outfp, "\tcheck_expected(%s);\n", show_ident(arg->ident));
		}
	} END_FOR_EACH_PTR(arg);
	fprintf(outfp, "}\n\n");

}

static const char *
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
		//strcpy(p, "struct ");
		//p += 7;
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

