# CC=gcc
CFLAGS:=-Wall -Wextra -Wpedantic -ggdb $(shell pkg-config --cflags ncurses readline)
LDLIBS=-I ${CHDRD} $(shell pkg-config --libs ncurses readline)
LEX:=flex

LEXD:=src/
LEXF:=$(shell find ${LEXD} -iname '*.l')
GENLEX:=$(subst .l,.c,${LEXF})

SRCD:=src/
OBJD:=obj/
SRC:=$(shell find ${SRCD} -iname '*.c') ${GENLEX}
OBJ:=$(subst .c,.o,$(subst ${SRCD},${OBJD},${SRC}))

HDRD:=${SRCD}
CHDRD:=${OBJD}
HDR:=$(shell find ${HDRD} -iname '*.h')
CHDR:=$(addsuffix .gch,$(subst ${HDRD},${CHDRD},${HDR}))

OUTPUT:=csope

main: ${CHDR} ${OBJ}
	${LINK.c} ${OBJ} -o ${OUTPUT} ${LDLIBS}

obj/%.o: src/%.c
	${COMPILE.c} $< -o $@

src/%.c: src/%.l
	${LEX} -o $@ $<

obj/%.h.gch: src/%.h
	${CC} $< -o $@

clean:
	-rm ${CHDR}
	-rm ${GENLEX}
	-rm ${OBJ}
	-rm ${OUTPUT}
