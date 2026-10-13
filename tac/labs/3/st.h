#ifndef ST_H
#define ST_H

typedef struct ST ST;

ST *st_create(int capacity);
int st_insert(ST *st, const char *lexeme);
int st_lookup(ST *st, const char *lexeme);
const char *st_get(ST *st, int pos);
int st_size(ST *st);
void st_free(ST *st);

#endif
