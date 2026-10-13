#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "st.h"

typedef struct Node {
    char *lexeme;
    int pos;
    struct Node *next;
} Node;

struct ST {
    Node **buckets;
    Node **by_pos;
    int capacity;
    int size;
};

static void *check(void *p)
{
    if (!p) {
        fprintf(stderr, "out of memory\n");
        exit(1);
    }
    return p;
}

static int hash(ST *st, const char *s)
{
    unsigned long h = 5381;
    while (*s)
        h = h * 33 + (unsigned char)*s++;
    return h % st->capacity;
}

static void alloc_buckets(ST *st, int capacity)
{
    st->capacity = capacity;
    st->buckets = check(calloc(capacity, sizeof(Node *)));
    st->by_pos = check(realloc(st->by_pos, capacity * sizeof(Node *)));
}

static void add_to_bucket(ST *st, Node *n)
{
    int h = hash(st, n->lexeme);
    n->next = st->buckets[h];
    st->buckets[h] = n;
}

static void rehash(ST *st)
{
    free(st->buckets);
    alloc_buckets(st, 2 * st->capacity + 1);
    for (int i = 0; i < st->size; i++)
        add_to_bucket(st, st->by_pos[i]);
}

ST *st_create(int capacity)
{
    ST *st = check(malloc(sizeof(ST)));
    st->by_pos = NULL;
    st->size = 0;
    alloc_buckets(st, capacity);
    return st;
}

int st_lookup(ST *st, const char *lexeme)
{
    for (Node *n = st->buckets[hash(st, lexeme)]; n; n = n->next)
        if (strcmp(n->lexeme, lexeme) == 0)
            return n->pos;
    return -1;
}

int st_insert(ST *st, const char *lexeme)
{
    int pos = st_lookup(st, lexeme);
    if (pos != -1)
        return pos;

    if (4 * (st->size + 1) > 3 * st->capacity)
        rehash(st);

    Node *n = check(malloc(sizeof(Node)));
    n->lexeme = strcpy(check(malloc(strlen(lexeme) + 1)), lexeme);
    n->pos = st->size;
    add_to_bucket(st, n);
    st->by_pos[st->size++] = n;
    return n->pos;
}

const char *st_get(ST *st, int pos)
{
    if (pos < 0 || pos >= st->size)
        return NULL;
    return st->by_pos[pos]->lexeme;
}

int st_size(ST *st)
{
    return st->size;
}

void st_free(ST *st)
{
    for (int i = 0; i < st->size; i++) {
        free(st->by_pos[i]->lexeme);
        free(st->by_pos[i]);
    }
    free(st->by_pos);
    free(st->buckets);
    free(st);
}
