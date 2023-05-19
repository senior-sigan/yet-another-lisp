# Yet another Lisp

I'm learning how to implement a (common) Lisp interpretator.

I do it in pure-c because I ~like pain~ found it fun.

## Steps

- Lexer state: Character index, line index, position in the line. Functions `peek` and `next`.
- Lexer loop:
  - space-like symbols: `' ' \n \r \f \v`
  - comments `;`
  - EOF - end of file
  - numbers: floats and integers
  - parentheses
  - symbols? identifiers? like `+` `defun` `let`
  - strings.

## Questions

- where to store tokens? In a heap?
- where to store Strings? In a heap?
- oh my GC implementation...
