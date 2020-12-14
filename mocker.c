
#define _GNU_SOURCE
#include <getopt.h>
#include <stdio.h>

#include "mocker.h"

// If set, we will generate mock data for this struct.
char *target_struct = NULL;

static int get_stream_id(const char *name)
{
	int i;
	for (i=0; i<input_stream_nr; i++) {
		if (strcmp(name, stream_name(i))==0)
			return i;
	}
	return -1;
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

	if (!sym) return;

	// Not a pointer -> Not a function pointer ;-)
	if (target_struct != NULL) {
		if (!is_ptr_type(sym)) {
			return;
		}
	}

	// Do we have a funtion behind that pointer
	// FIXME: wrap this in a helper 
	if (target_struct == NULL) {
		fn = sym->ctype.base_type;
	} else {
		fn = sym->ctype.base_type->ctype.base_type;
	}
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
//	if (sym->type != SYM_STRUCT) return;

//	if (target_struct && strcmp(target_struct, sym->ident->name) != 0) {
//		// Not the one...
//		return;
//	}

	printf("// File: %s %d\n", stream_name(sym->pos.stream), sym->pos.line);
	printf("struct %s\n", sym->ident->name);

	mock_members(sym->symbol_list);
}

static void cmocka_print(struct symbol *sym)
{
	mock_symbol(sym);
}

static void examine_namespace(struct symbol *sym)
{
	if (target_struct == NULL) {
		if (sym->ident) {
			if (sym->type == SYM_NODE)
				cmocka_print(sym);
			return;
		}
	}
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

static void
usage(const char *program)
{
	printf("Mocker Usage\n");
	printf("%s -o <outfile> -t <type> <infile>\n", program);
	printf("\nOptions:\n");
	printf(" -h help\n");
	printf(" -t <type> Type to target\n");
	exit(0);
}

int main(int argc, char **argv) {
	struct string_list *filelist = NULL;
	struct symbol_list *symlist = NULL;
	char *file;
	const char *optstring = "t:mhvco:I:";
	int cmocka;
	char *outfile;
	FILE *out = NULL;
	int make_mock = 0;
	int opt;
	const struct option options[] = {
		{ "cmocka", no_argument, &cmocka, 'c' },
		{ "type", required_argument, NULL, 't' },
		{ "mock", no_argument, &make_mock, 'm' },
		{ "verbose", no_argument, &verbose, 'v' },
		{ "outfile", required_argument, NULL, 'o' },
		{ "help", no_argument, NULL, 'h' }, 
		{ NULL, 0, 0, 0 },
	};

	printf("Mocker starting\n");

	while ((opt = getopt_long(argc, argv, optstring, options, NULL)) != -1) {
		switch (opt) {
		case 'c':
		case 'v':
		case 'm':
			break;
		case 't':
			printf("Target is %s\n", optarg);
			target_struct = optarg;
			break;
		case 'h':
			usage(argv[0]);
			break;
		case 'o':
			printf("Outputting to %s\n", optarg);
			outfile = optarg;
			break;
		default:
			printf("Unknown arg: %c\n", opt);
			exit(1);
		}
	}

	symlist = sparse_initialize(argc + 1 - optind, argv + optind - 1, &filelist);

	if (outfile) {
		out = fopen(outfile, "w");
		if (!out) {
			perror("fopen");
			exit(1);
		}
	}

	FOR_EACH_PTR_NOTAG(filelist, file) {
		examine_symbol_list(file, symlist);
		sparse_keep_tokens(file);
		examine_symbol_list(file, file_scope->symbols);
		examine_symbol_list(file, global_scope->symbols);
	} END_FOR_EACH_PTR_NOTAG(file);

	return 0;
}
