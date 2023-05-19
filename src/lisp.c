#include <readline/history.h>
#include <readline/readline.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
  int nop;
} Token;

typedef struct {
  size_t idx;
  size_t line;
  size_t line_idx;
  const char* text;
  size_t length;
} LexState;

LexState Lex_new(const char* text) {
  LexState lexer = {
      .idx = 0,
      .line = 0,
      .line_idx = 0,
      .text = text,
      .length = strlen(text),
  };
  return lexer;
}

void Lex_next(LexState* lex) {
  lex->idx += 1;
  lex->line_idx += 1;
}

void Lex_next_line(LexState* lex) {
  puts("BUM");
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
  if (dot_was_read) {
    double num = atof(literal) * sign;
    printf("FLOAT %lf\n", num);
  } else {
    long num = atol(literal) * sign;
    printf("INT %ld\n", num);
  }
  free(literal);
  // TODO: return a number token
  return NULL;
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
        read_minus(lex);
        break;
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
        read_number(lex, 1);
        break;
      default:
        printf("UNKNOWN token: %c\n", ch);
        Lex_next(lex);
        break;
    }
  }
}

int main(void) {
  const char* text = "(+ 14  2\n  (* 3. 9.7) -1 -3.14)";
  printf("======\n%s\n======\n", text);
  LexState lex = Lex_new(text);
  read_expr(&lex);

  return 0;
}
