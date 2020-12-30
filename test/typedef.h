
struct foo {
	int x;
};

typedef struct foo foo_t;

typedef struct { int y; } bar_t;

struct baz {
	int z;
};

int function(foo_t *arg);

foo_t function2(bar_t *arg);

void function3(struct baz);
