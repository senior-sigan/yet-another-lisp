#include <readline/history.h>
#include <readline/readline.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// The size of the heap in byte
#define MEMORY_SIZE 65536

typedef struct {
  void* memory;
  size_t offset;
} VM;

enum {
  TNIL = 0,
  TINT = 1,
  TFLOAT,
  TTRUE,
  TCPAREN,
  TLIST,
};

typedef struct {
  int type;
  union {
    long i_value;
    double f_value;
  };
} Token;

const Token* const True = &(Token){.type = TTRUE};
const Token* const Nil = &(Token){.type = TNIL};
const Token* const Cparen = &(Token){.type = TCPAREN};

// void gc(VM* vm) {
// TODO: see http://en.wikipedia.org/wiki/Cheney%27s_algorithm
// }

Token* alloc(VM* vm) {
  size_t size = sizeof(Token);
  // TODO: roundup and calculate size of the struct

  Token* token = (void*) ((char*) vm->memory + vm->offset);
  vm->offset += size;

  return token;
}

typedef struct {
  size_t idx;
  size_t line;
  size_t line_idx;
  const char* text;
  size_t length;
  VM* vm;
} LexState;

LexState Lex_new(const char* text, VM* vm) {
  LexState lexer = {
      .idx = 0,
      .line = 0,
      .line_idx = 0,
      .text = text,
      .length = strlen(text),
      .vm = vm,
  };
  return lexer;
}

void Lex_next(LexState* lex) {
  lex->idx += 1;
  lex->line_idx += 1;
}

void Lex_next_line(LexState* lex) {
  lex->line_idx = 0;
  lex->line += 1;
  lex->idx += 1;
}

char Lex_peek(const LexState* lex) {
  if (lex->idx >= lex->length) {
    return EOF;
  }
  return lex->text[lex->idx];
}

void skip_line(LexState* lex) {
  for (;;) {
    const char ch = Lex_peek(lex);
    if (ch == EOF) {
      return;
    } else if (ch == '\n' || ch == '\r') {
      Lex_next_line(lex);
      return;
    } else {
      Lex_next(lex);
    }
  }
}

bool is_space_like(char ch) {
  return ch == ' ' || ch == '\t' || ch == '\f' || ch == '\v' || ch == '\n' || ch == '\r' || ch == EOF;
}
bool is_number(char ch) {
  return ch >= '0' && ch <= '9';
}

Token* read_number(LexState* lex, int sign) {
  size_t start = lex->idx;
  size_t end = start;
  bool dot_was_read = false;
  for (;;) {
    const char ch = Lex_peek(lex);
    if (is_number(ch)) {
      // nothing to do
    } else if (ch == '.') {
      if (dot_was_read) {
        fprintf(stderr, "Syntax error at %lu:%lu: meet second dot while reading float\n", lex->line, lex->line_idx);
        return NULL;  // TODO: return parsing error token?
      } else {
        dot_was_read = true;
      }
    } else if (is_space_like(ch) || ch == ')' || ch == '(') {
      end = lex->idx;
      break;
    } else {
      // TODO: add exponential parsing 1e12, 1e-2, 1e+2, 1.2e+3
      fprintf(stderr, "Syntax error at %lu:%lu %c\n", lex->line, lex->line_idx, ch);
      return NULL;  // TODO: return parsing error token?
    }
    Lex_next(lex);
  }

  size_t length = end - start;
  char* literal = calloc(length + 1, sizeof(char));
  if (!literal) {
    // TODO: handle mem alloc failure
    fprintf(stderr, "Cannot allocate memory for literal\n");
    return NULL;
  }
  memcpy(literal, lex->text + start, length);

  Token* token = alloc(lex->vm);
  if (dot_was_read) {
    double num = atof(literal) * sign;
    token->type = TFLOAT;
    token->f_value = num;
    printf("FLOAT %lf\n", num);
  } else {
    long num = atol(literal) * sign;
    token->type = TINT;
    token->i_value = num;
    printf("INT %ld\n", num);
  }
  free(literal);
  // TODO: return a number token

  return token;
}

Token* read_minus(LexState* lex) {
  Lex_next(lex);
  char ch = Lex_peek(lex);
  if (is_number(ch)) {
    return read_number(lex, -1);
  }
  // TODO: read minus operator
  fprintf(stderr, "Minus operator is unsupported\n");
  return NULL;
}

const Token* read_expr(LexState* lex);

Token* read_list(LexState* lex) {
  printf("reading list\n");
  // (a b c d ...) == (a (b (c (d))))
  Lex_next(lex);  // read first (
  Token* res = alloc(lex->vm);
  res->type = TLIST;
  for (;;) {
    const Token* tk = read_expr(lex);
    if (tk == Cparen) {  // finally met a )
      return res;
    }
    if (tk == NULL) {
      // not closed (
      fprintf(stderr, "Unclosed list at EOF %lu:%lud\n", lex->line, lex->line_idx);
      return NULL;
    }
    // TODO: else do what? build a linked list?
  }
  return NULL;
}

const Token* read_expr(LexState* lex) {
  for (;;) {
    const char ch = Lex_peek(lex);
    switch (ch) {
      case EOF:
        return NULL;
      case ' ':
      case '\t':
      case '\v':
      case '\f':
        Lex_next(lex);
        break;
      case '\n':
      case '\r':
        Lex_next_line(lex);
        break;
      case ';':  // start of the comment
        skip_line(lex);
        break;
      case '-':
        return read_minus(lex);
      case '0':
      case '1':
      case '2':
      case '3':
      case '4':
      case '5':
      case '6':
      case '7':
      case '8':
      case '9':
        return read_number(lex, 1);
      case '(':
        return read_list(lex);
      case ')':
        return Cparen;
      default:
        printf("UNKNOWN token: %c\n", ch);
        Lex_next(lex);
        break;
    }
  }
}

void print_token(const Token* token) {
  if (!token) {
    printf("nil\n");
    return;
  }
  switch (token->type) {
    case TINT:
      printf("%ld\n", token->i_value);
      break;
    case TFLOAT:
      printf("%lf\n", token->f_value);
      break;
  }
}

int main(void) {
  VM vm = {
      .offset = 0,
      .memory = malloc(MEMORY_SIZE),
  };
  if (!vm.memory) {
    fprintf(stderr, "Failed to allocate memory for VM\n");
    exit(1);
  }
  const char* text = "(+ 14  2\n  (* 3. 9.7) -1 -3.14)";
  printf("======\n%s\n======\n", text);
  LexState lex = Lex_new(text, &vm);
  print_token(read_expr(&lex));
  free(vm.memory);

  return 0;
}
