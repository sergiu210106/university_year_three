# seminar 2

## token = one word from the language -> the smallest piece that means something

### 5 types:
- fixed
    - reserved words: VAR, BEGIN, WHILE, READ
    - operators: + - = <= =>
    - separators: ( ) ; : .
- defined by programmer
    - identifiers: `a`, `sum`, `temp`
    - constants: `10` `"abc"`

## Scanner (lexical analysis)
- the first phase of a compiler
- cuts the program in tokens and clasifies them
- gets as input the source program and the tokens file
- outputs PIF, ST, lexical errors
- the tokens file contains every fixed token 
- ST (**SYMBOL TABLE**) contains all identifiers and constants, each with a position
- PIF (**PROGRAM INTERNAL FORM**) 
    - `token, ST_pos`
    - for identifier token is id, for constant is const
    - st pos is the value in st, or 0 if not there

- detect tokens, clasify, codify for identifiers and constants

`x=x+10;` => 6 tokens: x, =, x, +, 10, ;

PIF: 
    - token : id, =, id, +,10,;
    - st_pos: 1, 0, 1, 0,2,0

SF:
    - st_pos: 1,2
    - symbol: x,10

### Exercise 1
*Generate the PIF and ST*
```fortran
BEGIN
    READ(a);
    READ(b);
    IF a>b then
        WRITE(a)
    ELSE
        WRITE(b)
END.
```

- **lexical errors**: a token that cannot be classified
    - 1a, 007, "ab, # (if not in alphabet)

### Exercise 2
```fortran
BEGIN
    READ(x);
    
```




