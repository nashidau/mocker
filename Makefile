
CFLAGS+=-I../sparse
LDFLAGS+=../sparse/libsparse.a -L../sparse

fraud: fraud.o
	${CC} ${CFLAGS} -o fraud fraud.o ${LDFLAGS}

mocker: mocker.o
	${CC} ${CFLAGS} -o mocker mocker.o ${LDFLAGS}

.PHONY: test
test: mocker fraud
	./mocker -c test/cmocka.h
	./fraud -t test/template.fraud -o out.c test/cmocka.h
