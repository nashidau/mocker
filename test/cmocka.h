
struct foo;

struct bar {
	int x;
	int y;
};

typedef struct bar bar_t;
typedef struct { int x; } xxx_t;

int cmocka_testa(int a, int b, int c);
int cmocka_testb(double a, float x);
int cmocka_test_retrun(int a, void *x, void **result);
void *cmock_testc(void *x, void ******y);
struct foo *cmock_testd(void);
struct bar *cmock_teste(struct foo *foo);
bar_t *cmock_testf(struct foo *foo);
xxx_t *cmock_testg(struct foo *foo);
