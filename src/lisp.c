#include <ctype.h>
#include <readline/history.h>
#include <readline/readline.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// The size of the heap in byte
#define MEMORY_SIZE 65536

const char symbol_chars[] = "*+-/:<=>";

bool is_symbol_chars(char ch) {
  return isalpha(ch) || strchr(symbol_chars, ch);
}

typedef struct {
  void* memory;
  size_t used;
} VM;

enum {
  TNIL = 0,
  TINT = 1,
  TFLOAT,
  TTRUE,
  TCPAREN,
  TCELL,
  TSYMBOL,
};

typedef struct Token_ {
  int type;
  size_t size;
  union {
    long i_value;
    double f_value;
    struct {
      struct Token_* car;
      struct Token_* cdr;
    };
    char name[1];  // this array will be the token.size length! see alloc
  };
} Token;

Token* const True = &(Token){.type = TTRUE};
Token* const Nil = &(Token){.type = TNIL};
Token* const Cparen = &(Token){.type = TCPAREN};

// void gc(VM* vm) {
// TODO: see http://en.wikipedia.org/wiki/Cheney%27s_algorithm
// }

// Round up the given value to a multiple of size. Size must be a power of 2. It adds size - 1
// first, then zero-ing the least significant bits to make the result a multiple of size. I know
// these bit operations may look a little bit tricky, but it's efficient and thus frequently used.
static inline size_t roundup(size_t var, size_t size) {
  return (var + size - 1) & ~(size - 1);
}

Token* alloc(VM* vm, size_t size) {
  size = roundup(size, sizeof(void*));
  size += offsetof(Token, i_value);
  size = roundup(size, sizeof(void*));

  if (MEMORY_SIZE < vm->used + size) {
    fprintf(stderr, "Memory exhausted");
    return NULL;
  }

  Token* token = (Token*) ((char*) vm->memory + vm->used);
  token->size = size;
  vm->used += size;

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

char Lex_peek_next(const LexState* lex) {
  if (lex->idx + 1 >= lex->length) {
    return EOF;
  }
  return lex->text[lex->idx + 1];
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

Token* read_symbol(LexState* lex) {
  // 1. calculate symbol length
  size_t start = lex->idx;
  size_t end = start;
  for (;;) {
    const char ch = Lex_peek(lex);
    bool allowed = isdigit(ch) || is_symbol_chars(ch);
    if (!allowed) {
      end = lex->idx;
      break;
    }
    Lex_next(lex);
  }
  size_t length = end - start;
  char* symbol_name = calloc(length + 1, sizeof(char));
  if (!symbol_name) {
    // TODO: handle mem alloc failure
    fprintf(stderr, "Cannot allocate memory for symbol name\n");
    return NULL;
  }
  memcpy(symbol_name, lex->text + start, length);
  Token* token = alloc(lex->vm, length + 1);
  token->type = TSYMBOL;
  strcpy(token->name, symbol_name);
  printf("SYM %s\n", symbol_name);
  free(symbol_name);
  return token;
}

Token* read_number(LexState* lex, int sign) {
  size_t start = lex->idx;
  size_t end = start;
  bool dot_was_read = false;
  for (;;) {
    const char ch = Lex_peek(lex);
    if (isdigit(ch)) {
      // nothing to do
    } else if (ch == '.') {
      if (dot_was_read) {
        fprintf(stderr, "Syntax error at %lu:%lu: meet second dot while reading float\n", lex->line, lex->line_idx);
        return NULL;  // TODO: return parsing error token?
      } else {
        dot_was_read = true;
      }
    } else if (isspace(ch) || ch == EOF || ch == ')' || ch == '(') {
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

  Token* token = alloc(lex->vm, sizeof(double));
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

  return token;
}

Token* read_minus(LexState* lex) {
  Lex_next(lex);
  char ch = Lex_peek(lex);
  if (isdigit(ch)) {
    return read_number(lex, -1);
  }
  // impossible?
  return NULL;
}

Token* read_expr(LexState* lex);

Token* cons(VM* vm, Token* car, Token* cdr) {
  Token* cell = alloc(vm, sizeof(Token*) * 2);
  cell->type = TCELL;
  cell->car = car;
  cell->cdr = cdr;
  return cell;
}

Token* read_list(LexState* lex) {
  printf("reading list\n");
  // (a b c d ...) == (a (b (c (d))))
  Lex_next(lex);  // read first (
  Token* head = NULL;
  Token* res = head;
  for (;;) {
    Token* tk = read_expr(lex);
    if (tk == Cparen) {  // finally met a )
      Lex_next(lex);
      return res;
    }
    if (tk == NULL) {
      // not closed (
      fprintf(stderr, "Unclosed list at EOF %lu:%lud\n", lex->line, lex->line_idx);
      return NULL;
    }
    if (!head) {
      head = cons(lex->vm, tk, NULL);
      res = head;
    } else {
      head->cdr = cons(lex->vm, tk, NULL);
      head = head->cdr;
    }
  }
  // impossible!!!
  return NULL;
}

Token* read_expr(LexState* lex) {
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
        if (isdigit(Lex_peek_next(lex))) {
          return read_minus(lex);
        }  // otherwise it is an minus operator
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
        if (is_symbol_chars(ch)) {
          return read_symbol(lex);
        } else {
          printf("UNKNOWN token: %c\n", ch);
          Lex_next(lex);
          return Nil;
        }
    }
  }
}

void print_token(Token* token) {
  if (!token) {
    printf("nil");
    return;
  }
  switch (token->type) {
    case TINT:
      printf("%ld", token->i_value);
      break;
    case TNIL:
      printf("nil");
      break;
    case TFLOAT:
      printf("%lf", token->f_value);
      break;
    case TCELL:
      printf("(");
      print_token(token->car);
      printf(" ");
      print_token(token->cdr);
      printf(")");
      break;
    case TSYMBOL:
      printf("%s", token->name);
      break;
    default:
      fprintf(stderr, "???");
      break;
  }
}

Token* eval(LexState* lex, Token* token) {
  if (!token) {
    return NULL;
  }
  switch (token->type) {
    case TCELL:
      // TODO: eval list?
      return NULL;
    case TSYMBOL:
      if (strcmp(token->name, "+")) {
        double sum = 0;
        // for (Token* args = eval_list(); args != NULL; args = args->cdr) {
        //   if (args->car->type == TFLOAT) {
        //     sum += args->car->f_value;
        //   } else if (args->car->type == TINT) {
        //     sum += args->car->i_value;
        //   } else {
        //     fprintf(stderr, "expected int or float got %s\n", args->car->type);
        //     return NULL;
        //   }
        // }
        Token* token = alloc(lex->vm, sizeof(double));
        token->type = TFLOAT;
        token->f_value = sum;
        return token;
      } else {
        fprintf(stderr, "Unsupported symbol %s\n", token->name);
        return NULL;
      }
      break;
    default:
      return token;
      break;
  }
}

int main(void) {
  VM vm = {
      .used = 0,
      .memory = malloc(MEMORY_SIZE),
  };
  if (!vm.memory) {
    fprintf(stderr, "Failed to allocate memory for VM\n");
    exit(1);
  }
  // const char* text = "(print (+ 14  2 (* 3 9.7) -1 -3.14))";
  // const char* text = "(1 2 3 4 5 6)";
  // const char* text = "(print -42)";
  // const char* text = "(* (+ 1 2) 3)";
  const char* text = "(+ 1 2 3)";
  printf("======\n%s\n======\n", text);
  LexState lex = Lex_new(text, &vm);
  print_token(read_expr(&lex));
  free(vm.memory);

  return 0;
}
