CPPFLAGS = $(shell ncursesw6-config --cflags)
CFLAGS   = -ggdb -std=c89 -Wextra -Wall ${CPPFLAGS} 
LDFLAGS  = $(shell ncursesw6-config --libs)

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
