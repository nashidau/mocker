/**
 * Generate a cmocka style mock from a template file.
 *
 * Usage:
 *  fraud -t template.fraud -o mock.c header.h <sparse options>
 *
 *   --include Include.path
 *
 * Templates should contain the following lines
 * ** INCLUDES (at the start of the line).
 *
 * ** MOCKS  
 *
 * If no template is given a very simple default is created.
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

// How much debug to spew to stdout.
// 0 is none, higher is more and more to 100
static int verbosity = 0;

struct include {
	struct include *next;
	const char *include;
};


static void examine_symbol_list(const char *file, struct symbol_list *list, FILE *outfp);
static void examine_namespace(struct symbol *sym, FILE *outfp);
static void mock_symbol(struct symbol *sym, FILE *outfp);
static const char *get_typename(struct ctype *ctype);
static void dump_mocks(struct string_list *filelist, struct symbol_list *symlist, FILE *outfp);
static void print_symbol(struct symbol *sym, int indent);

static void
usage(const char *argv0)
{
	puts("fraud: Generate a cmocka mock from a header file.");
	puts("");
	printf(" %s <-t template> -o <output.c> <inputfile> [sparse options]\n", argv0);
	printf(" -v Print lots of verbose debugging\n");
}

/** Returns a constant string of spaces which is a prefix.
 *
 */
static const char *
get_space_prefix(int prefix) {
	static const char *spaces = "          " "          " "          " "          ";
	if (prefix * 2 > sizeof(spaces))
		return spaces;
	return spaces + 40 - 2 * prefix;
}

static int get_stream_id(const char *name)
{
	int i;
	for (i=0; i<input_stream_nr; i++) {
		if (strcmp(name, stream_name(i))==0)
			return i;
	}
	return -1;
}

/**
 * Get the name of the Nth argument to a function.
 * If there is not identifier, return a pregenerated 'argN' string.
 */
static const char *
get_arg_name(int n, const struct ident *ident)
{
	const char *argN[11] = { 
		"arg0", "arg1", "arg2", "arg3", "arg4", "arg5",
		"arg6", "arg7", "arg8", "arg8", "arg9",
	};


	if (ident) {
		return ident->name;
	}
	if (n < 11) {
		return argN[n];
	}
	// Leaks a little; but rare
	// FIXME: use a non-GNU asprintf (custom function here)
	//asprintf(&tmp, "arg%d", n);
	return "pants";
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
	const char *optstring = "t:o:i:hv";
	const struct option options[] = {
		{ "template", required_argument, NULL, 't' },
		{ "output", required_argument, NULL, 'o' },
		{ "include", required_argument, NULL, 'i' },
		{ "help", no_argument, NULL, 'h' },
		{ "verbose", no_argument, NULL, 'v' },
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
		case 'h':
			usage(argv[0]);
			exit(0);
			break;
		case 'v':
			verbosity ++;
			break;
		default:
			printf("Unkown Arg: %c\n", opt);
			exit(1);
		}
	}

	if (template == NULL || output == NULL) {
		printf("Need template & output\n");
		usage(argv[0]);
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
	int arg_index;

	if (!sym) return;

	if (verbosity > 0) print_symbol(sym, 0);

	fn = sym->ctype.base_type;
	
	if (fn->type != SYM_FN) {
		return;
	}

	// So we have a function pointer:
	fprintf(outfp, "%s\n%s(\n", get_typename(&fn->ctype), show_ident(sym->ident));

	arg_index = 1;
	FOR_EACH_PTR(fn->arguments, arg) {
		fprintf(outfp, "%s\t\t%s %s", arg_index == 1 ? "": ",\n", 
				get_typename(&arg->ctype), get_arg_name(arg_index, arg->ident));
		if (verbosity > 0) {
			printf("Arg %d\n", arg_index);
			print_symbol(sym, 0);
		}
		arg_index ++;
	} END_FOR_EACH_PTR(arg);

	// Function had no params, print 'void'.
	if (arg_index == 1) {
		fprintf(outfp, "\t\tvoid");
	}
	fprintf(outfp, ") {\n");

	arg_index = 1;
	FOR_EACH_PTR(fn->arguments, arg) {
		if (strcmp(get_arg_name(arg_index, arg->ident), "result") == 0) {
			fprintf(outfp, "\t{\n");
			fprintf(outfp, "\t\t%s rv = mock_ptr_type(%s));\n",
					get_typename(&arg->ctype),
					get_typename(&arg->ctype));
			fprintf(outfp, "\t\tif (%s != NULL) {\n", get_arg_name(arg_index, arg->ident));
			fprintf(outfp, "\t\t\tmemcpy(%s, rv, sizeof(*%s));\n",
					get_arg_name(arg_index, arg->ident), get_arg_name(arg_index, arg->ident));
			fprintf(outfp, "\t\t}\n\t}\n");
		} else if (arg->ctype.base_type->type == SYM_PTR) {
			fprintf(outfp, "\tcheck_expected_ptr(%s);\n", get_arg_name(arg_index, arg->ident));
		} else {
			fprintf(outfp, "\tcheck_expected(%s);\n", get_arg_name(arg_index, arg->ident));
		}
		arg_index ++;
	} END_FOR_EACH_PTR(arg);

	// Print the return, if not void
	// FIXME: Should check the basetype and use that.
	if (strcmp(get_typename(&fn->ctype), "void") != 0) {
		fprintf(outfp, "\treturn mock_type(%s);\n", get_typename(&fn->ctype));
	}

	fprintf(outfp, "}\n\n");

}

static void
print_ctype(struct ctype *ctype, int indent) {
	const char *prefix = get_space_prefix(indent);
	printf("%s##ctype: %p\n", prefix, ctype);
	if (ctype->base_type) {
		printf("%s<<basetype:",prefix); print_symbol(ctype->base_type, indent + 1);
		printf("%s>>\n    modifiers: %lu  Alignment %lu\n", prefix, ctype->modifiers, ctype->alignment);
	} else {
		printf("%s    modifiers: %lu  Alignment %lu\n", prefix, ctype->modifiers, ctype->alignment);
	}
}

static void
print_symbol(struct symbol *sym, int indent) {
	const char *prefix = get_space_prefix(indent);
	printf("%s##sym %p\n", prefix, sym);
	printf("%s  type: %d\n", prefix, sym->type);
	printf("%s  namespace: %d\n", prefix, sym->namespace);
	printf("%s  used: %d Attr: %d enum_member: %d bound: %d\n",
			prefix, sym->used, sym->attr, sym->enum_member, sym->bound);
	printf("%s  ident: %p/%s\n", prefix, sym->ident, show_ident(sym->ident));
	printf("%s  next: %p\n", prefix, sym->next_id);
	print_ctype(&sym->ctype, indent);
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
	} else if (ctype->base_type->type == SYM_STRUCT ||
			ctype->base_type->type == SYM_UNION) {
		if (ctype->base_type->namespace == NS_STRUCT) {
			if (ctype->base_type->type == SYM_UNION) {
				strcpy(p, "union ");
				p += 6;
			} else {
				strcpy(p, "struct ");
				p += 7;
			}
		}
		strcpy(p, ctype->base_type->ident->name);
		p += ctype->base_type->ident->len;
	} else {
		printf("Unknown type");
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

