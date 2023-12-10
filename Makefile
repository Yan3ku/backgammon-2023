DEBUG    = -fsanitize-undefined-trap-on-error -fsanitize=address,undefined -g3 # -fsanitize-trap=all
CPPFLAGS = $(shell ncursesw6-config --cflags)
CFLAGS   = -std=c99 -pedantic -Wextra -Wall ${CPPFLAGS} ${DEBUG}
LDFLAGS  = $(shell ncursesw6-config --libs) ${DEBUG}

BIN = prog
SRC = prog.c
OBJ = ${SRC:.c=.o}

all: options ${BIN}

options:
	@echo ${BIN} build options:
	@echo "CFLAGS   = ${CFLAGS}"
	@echo "LDFLAGS  = ${LDFLAGS}"
	@echo "CC       = ${CC}"

.c.o:
	${CC} -c ${CFLAGS} $<

${BIN}: ${OBJ}
	${CC} -o $@ ${OBJ} ${LDFLAGS}

clean:
	rm -f ${BIN} ${OBJ}

.PHONY: all options clean
