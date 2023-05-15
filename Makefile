CC= clang
RM= rm -f

CFLAGS= -Wall -Wextra -Werror -pedantic -std=c11 -ggdb
LIBS= -lm

SRC= src/lisp.c

all: lisp
	
lisp: $(SRC)
	$(CC) $(CFLAGS) -o lisp $(SRC) $(LIBS)

.PHONY: clean
clean:
	$(RM) lisp

.PHONY: echo
echo:
	@echo "CC = $(CC)"
	@echo "RM = $(RM)"
	@echo "CFLAGS = $(CFLAGS)"
	@echo "LIBS = $(LIBS)"