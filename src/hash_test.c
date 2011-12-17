#include <stdio.h>
#include "hash.h"


int main ()
{
    h_hash_t *h;
    h_item_t *e;

    h = h_init(128);
    /*
    h_insert(h, "first_name", "Hasan");
    printf("first name is: %s\n", h_get(h, "first_name"));
    h_remove(h, "first_name");
    printf("first name is: %s\n", h_get(h, "first_name"));
    h_remove(h, "first_name");
    printf("first name is: %s\n", h_get(h, "first_name"));

    */
    h_insert(h, "first_name", "Hasan");
    h_insert(h, "last_name", "Alayli");
    h_insert(h, "age", "27");
    h_insert(h, "location", "san francisco");
    while ((e = h_next(h)) != NULL){
        printf("entry is: %s: %s\n", e->key, e->value);
    }

    h_free(h);
    return 0;
}
