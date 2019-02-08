#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "family.h"

/* Number of word pointers allocated for a new family.
   This is also the number of word pointers added to a family
   using realloc, when the family is full.
*/
static int family_increment = 0;


/* Set family_increment to size, and initialize random number generator.
   The random number generator is used to select a random word from a family.
   This function should be called exactly once, on startup.
*/
void init_family(int size) {
    family_increment = size;
    srand(time(0));
}


/* Given a pointer to the head of a linked list of Family nodes,
   print each family's signature and words.

   Do not modify this function. It will be used for marking.
*/
void print_families(Family* fam_list) {
    int i;
    Family *fam = fam_list;
    
    while (fam) {
        printf("***Family signature: %s Num words: %d\n",
               fam->signature, fam->num_words);
        for(i = 0; i < fam->num_words; i++) {
            printf("     %s\n", fam->word_ptrs[i]);
        }
        printf("\n");
        fam = fam->next;
    }
}


/* Return a pointer to a new family whose signature is 
   a copy of str. Initialize word_ptrs to point to 
   family_increment+1 pointers, numwords to 0, 
   maxwords to family_increment, and next to NULL.
*/
Family *new_family(char *str) {
    Family *new_family = malloc(sizeof(Family));
    new_family->signature = str;
    new_family->word_ptrs = malloc((family_increment + 1) * sizeof(char *));
    new_family->num_words = 0;
    new_family->max_words = family_increment;
    new_family->next = NULL;
    return new_family;
}


/* Add word to the next free slot fam->word_ptrs.
   If fam->word_ptrs is full, first use realloc to allocate family_increment
   more pointers and then add the new pointer.
*/
void add_word_to_family(Family *fam, char *word) {
    if (fam->num_words == fam->max_words) {
        fam->word_ptrs = realloc(fam->word_ptrs, family_increment * sizeof(char *));
    }
    fam->word_ptrs[fam->num_words] = word;
    //might need to make that index fam->num_words + 1 = NULL
    fam->num_words++;
    fam->max_words += family_increment;
    return;
}


/* Return a pointer to the family whose signature is sig;
   if there is no such family, return NULL.
   fam_list is a pointer to the head of a list of Family nodes.
*/
Family *find_family(Family *fam_list, char *sig) {
    Family *curr_fam = fam_list;
    while (curr_fam) {
        if (curr_fam->signature == sig) { //string comparison? char [] comparison?
            return curr_fam;
        }
        curr_fam = curr_fam->next;
    }
    return NULL;
}


/* Return a pointer to the family in the list with the most words;
   if the list is empty, return NULL. If multiple families have the most words,
   return a pointer to any of them.
   fam_list is a pointer to the head of a list of Family nodes.
*/
Family *find_biggest_family(Family *fam_list) {
    Family *curr_fam = fam_list;
    Family *curr_max = fam_list;
    while (curr_fam) {
        if (curr_fam->num_words > curr_max->num_words) {
            curr_max = curr_fam;
        }
        curr_fam = curr_fam->next;
    }
    if (curr_max) {
        return curr_max;
    }
    return NULL;
}


/* Deallocate all memory rooted in the List pointed to by fam_list. */
void deallocate_families(Family *fam_list) {
    Family *curr_fam = fam_list;
    Family *next_fam = NULL;
    while (curr_fam) {
        next_fam = curr_fam->next;
        free(curr_fam->word_ptrs);
        free(curr_fam->signature);
        free(curr_fam);
        curr_fam = next_fam;
    }
    return;
}


/* 
   Get the signature of the word 
*/
char *get_signature_of_word(char *word, int size, char letter) {
    char *signature = malloc((size + 1) * sizeof(char)); //maybe dont need to malloc
    signature[size] = '\0';
    for (int i = 0; i < size; i++) {
        if (word[i] != letter) {
            signature[i] = '-';
        } else {
            signature[i] = letter;
        }
    }
    return signature;
}


/* Generate and return a linked list of all families using words pointed to
   by word_list, using letter to partition the words.

   Implementation tips: To decide the family in which each word belongs, you
   will need to generate the signature of each word. Create only the families
   that have at least one word from the current word_list.
*/
Family *generate_families(char **word_list, char letter) {
    char *signature;
    int len = 0;
    Family *first_fam = NULL; //head of the family linked list
    Family *fam; // for getting the family that the current word is in
    Family *last_fam; // for iterating to the last family in family linked list
    if (word_list[0] != NULL) {
        len = strlen(word_list[0]);
        signature = get_signature_of_word(word_list[0], len, letter);
        first_fam = new_family(signature);
        add_word_to_family(first_fam, word_list[0]);
        last_fam = first_fam;

        for (int i = 1; word_list[i] != NULL; i++) {
            signature = get_signature_of_word(word_list[i], len, letter);
            fam = find_family(first_fam, signature);
            if (fam != NULL) {
                add_word_to_family(fam, word_list[i]);
            } else {
                last_fam->next = new_family(signature);
                last_fam = last_fam->next;
                add_word_to_family(last_fam, word_list[i]);
            }
        }
    }
    return first_fam;
}


/* Return the signature of the family pointed to by fam. */
char *get_family_signature(Family *fam) {
    if (fam) {
        return fam->signature;
    }
    return NULL;
}


/* Return a pointer to word pointers, each of which
   points to a word in fam. These pointers should not be the same
   as those used by fam->word_ptrs (i.e. they should be independently malloc'd),
   because fam->word_ptrs can move during a realloc.
   As with fam->word_ptrs, the final pointer should be NULL.
*/
char **get_new_word_list(Family *fam) {
    char **new_word_list = malloc((fam->num_words + 1) * sizeof(char *));
    for (int i = 0; i < fam->num_words; i++) {
        new_word_list[i] = fam->word_ptrs[i];
    }
    new_word_list[fam->num_words] = NULL;
    return new_word_list;
}


/* Return a pointer to a random word from fam. 
   Use rand (man 3 rand) to generate random integers.
*/
char *get_random_word_from_family(Family *fam) {
    if (fam != NULL) {
        int random = rand() % fam->num_words;
        return fam->word_ptrs[random];
    }
    return NULL;
}
