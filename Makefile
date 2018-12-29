
CFLAGS+=-I../sparse
LDFLAGS+=../sparse/libsparse.a -L../sparse

mocker: mocker.o
	${CC} ${CFLAGS} -o mocker mocker.o ${LDFLAGS}

