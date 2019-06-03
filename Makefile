
CFLAGS+=-I../sparse
LDFLAGS+=../sparse/libsparse.a -L../sparse

fraud: fraud.o
	${CC} ${CFLAGS} -o fraud fraud.o ${LDFLAGS}

mocker: mocker.o
	${CC} ${CFLAGS} -o mocker mocker.o ${LDFLAGS}

test: mocker
	./mocker -c test/cmocka.h

