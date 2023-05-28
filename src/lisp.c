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

// https://stackoverflow.com/questions/1701055/what-is-the-maximum-length-in-chars-needed-to-represent-any-double-value
// a buffer to store a double/long numbers for atoi and atof functions
static char number_buffer[1079];

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
  TBOOL,
  TCPAREN,
  TCELL,
  TSYMBOL,
  TSTRING,
  TFUNCTION,
};

struct Token_;
typedef struct Token_* Func(VM* vm, struct Token_* args);

typedef struct Token_ {
  int type;
  size_t size;
  union {
    bool b_value;
    long i_value;
    double f_value;
    Func* fun;
    struct {
      struct Token_* car;
      struct Token_* cdr;
    };
    char name[1];  // this array will be the token.size length! see alloc
  };
} Token;

Token* const True = &(Token){.type = TBOOL, .b_value = true};
Token* const False = &(Token){.type = TBOOL, .b_value = false};
Token* const Nil = &(Token){.type = TNIL};
Token* const Cparen = &(Token){.type = TCPAREN};

Token* add(VM* vm, Token* args) {
  return NULL;
}

Token* const Add = &(Token){.type = TFUNCTION, .fun = add};

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
      // TODO: check for a space-like character to stop parsing
      end = lex->idx;
      break;
    }
    Lex_next(lex);
  }
  size_t length = end - start;
  Token* token = alloc(lex->vm, length + 1);
  token->type = TSYMBOL;
  strncpy(token->name, lex->text + start, length);
  token->name[length] = '\0';
  printf("[DEBUG] SYM %s\n", token->name);
  return token;
}

Token* read_number(LexState* lex, int sign) {
  bool dot_was_read = false;
  size_t i = 0;
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
      break;
    } else {
      // TODO: add exponential parsing 1e12, 1e-2, 1e+2, 1.2e+3
      fprintf(stderr, "Syntax error at %lu:%lu %c\n", lex->line, lex->line_idx, ch);
      return NULL;  // TODO: return parsing error token?
    }
    number_buffer[i++] = ch;
    Lex_next(lex);
  }

  number_buffer[i] = '\0';

  Token* token;
  if (dot_was_read) {
    token = alloc(lex->vm, sizeof(double));
    double num = atof(number_buffer) * sign;
    token->type = TFLOAT;
    token->f_value = num;
    printf("[DEBUG] FLOAT %lf\n", num);
  } else {
    token = alloc(lex->vm, sizeof(long));
    long num = atol(number_buffer) * sign;
    token->type = TINT;
    token->i_value = num;
    printf("[DEBUG] INT %ld\n", num);
  }

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

Token* read_string(LexState* lex) {
  Lex_next(lex);  // read first "
  // 1. calculate string length
  size_t start = lex->idx;
  size_t end = start;
  for (;;) {
    const char ch = Lex_peek(lex);
    if (ch == '"') {
      end = lex->idx;
      Lex_next(lex);
      break;
    }
    if (ch == EOF) {
      fprintf(stderr, "Unclosed string at %lu:%lu\n", lex->line, lex->line_idx);
      return NULL;
    }
    Lex_next(lex);
  }
  size_t length = end - start;
  Token* token = alloc(lex->vm, length + 1);
  token->type = TSTRING;
  strncpy(token->name, lex->text + start, length);
  token->name[length] = '\0';
  printf("[DEBUG] STR %s\n", token->name);
  return token;
}

Token* read_list(LexState* lex) {
  printf("[DEBUG] reading list\n");
  // (a b c d ...) == (a (b (c (d nil))))
  Lex_next(lex);  // read first (
  Token* head = NULL;
  Token* tail = NULL;
  for (;;) {
    Token* tk = read_expr(lex);
    if (tk == Cparen) {  // finally met a )
      Lex_next(lex);
      return head;
    }
    if (tk == NULL) {
      // not closed (
      fprintf(stderr, "Unclosed list at EOF %lu:%lud\n", lex->line, lex->line_idx);
      return NULL;
    }
    if (!head) {
      head = cons(lex->vm, tk, NULL);
      tail = head;
    } else {
      tail->cdr = cons(lex->vm, tk, NULL);
      tail = tail->cdr;
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
      case '"':
        return read_string(lex);
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
      printf(" . ");
      print_token(token->cdr);
      printf(")");
      break;
    case TSYMBOL:
      printf("%s", token->name);
      break;
    case TSTRING:
      printf("\"%s\"", token->name);
      break;
    default:
      fprintf(stderr, "???");
      break;
  }
}

Token* find(VM* vm, const char* symbol) {
  if (strcmp(symbol, "+")) {
    return Add;
  }
  return NULL;
}

Token* eval(VM* vm, Token* token);

/**
 * Apply function with arguments.
 * args is a con of arguments.
 */
Token* apply(VM* vm, Token* fun, Token* args) {
  // TODO: iterate over args and apply fun like reduce?
  return NULL;
}

/**
 * Expecting the CAR to be a function identifier
 * and CDR to be a list of arguments.
 */
Token* eval_cell(VM* vm, Token* token) {
  Token* sym = token->car;
  Token* args = token->cdr;
  if (sym->type != TSYMBOL) {
    fprintf(stderr, "[ERROR] expected a function name, got %d:\n\t", sym->type);
    print_token(sym);
    printf("\n");

    return NULL;
  }

  Token* fn = find(vm, sym->name);
  if (fn->type != TFUNCTION) {
    fprintf(stderr, "[ERROR] expected a function, got %d:\n\t", fn->type);
    print_token(fn);
    printf("\n");
  }

  return apply(vm, fn, args);
}

Token* eval(VM* vm, Token* token) {
  if (!token) {
    return NULL;
  }
  switch (token->type) {
    case TCELL:
      return eval_cell(vm, token);
    case TSYMBOL:
      // TODO: find token->name in the environment and return it
      fprintf(stderr, "Unsupported symbol %s\n", token->name);
      return NULL;
    default:
      return token;
      break;
  }
}

void execute(const char* text) {
  VM vm = {
      .used = 0,
      .memory = malloc(MEMORY_SIZE),
  };
  if (!vm.memory) {
    fprintf(stderr, "Failed to allocate memory for VM\n");
    exit(1);
  }
  printf("======\n%s\n======\n", text);
  LexState lex = Lex_new(text, &vm);
  eval(lex.vm, read_expr(&lex));
  free(vm.memory);
}

int main(void) {
  execute("(1 2 3 4 5)");
  execute("(+ 1 2 3 4 5 6)");
  // execute("(* (+ 1 2) 3)");
  // execute("(print (+ 14  2 (* 3 9.7) -1 -3.14))");
  // execute("(print -42)");
  // execute("(let hello \"world\" a 42)");

  return 0;
}
