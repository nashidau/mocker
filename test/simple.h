
struct bar;

int x;
int y;

struct foo {
	int x; // Should skip
	float f;
	int *y; // Should skip
	int (*getx)(struct foo *foo);
	int (*setx)(struct foo *foo, int x);
	float (*getf)(struct foo *foo, float y);
	double (*setf)(struct foo *foo, double y);
	int (*stuff)(int *x, int *y);
	struct foo *(*clone)(struct foo *);
	struct bar *(*mutate)(struct foo *);
	int (*merge)(struct foo *, struct bar *);
	int *(*intptr)(struct foo *);
	int **(*intptrptr)(struct foo *);
	int ***(*intptr3)(struct foo *);
	int ****(*intptr4)(struct foo *);
	int *****(*intptr5)(struct foo *);
};
