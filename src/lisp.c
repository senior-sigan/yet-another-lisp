#include <readline/history.h>
#include <readline/readline.h>
#include <stdio.h>
#include <stdlib.h>

int main(void) {
  for (;;) {
    char* line = readline(">>> ");
    if (!line) return 0;
    add_history(line);

    puts(line);

    free(line);
  }

  return 0;
}
