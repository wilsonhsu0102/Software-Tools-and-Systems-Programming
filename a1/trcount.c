#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

// Constants that determine that address ranges of different memory regions

#define GLOBALS_START 0x400000
#define GLOBALS_END   0x700000
#define HEAP_START   0x4000000
#define HEAP_END     0x8000000
#define STACK_START 0xfff000000

int main(int argc, char **argv) {
    
    FILE *fp = NULL;

    if(argc == 1) {
        fp = stdin;

    } else if(argc == 2) {
        fp = fopen(argv[1], "r");
        if(fp == NULL){
            perror("fopen");
            exit(1);
        }
    } else {
        fprintf(stderr, "Usage: %s [tracefile]\n", argv[0]);
        exit(1);
    }

    /* Complete the implementation */
    unsigned long address;
    char operation;
    int num_instructions = 0;
    int num_modifies = 0;
    int num_loads = 0;
    int num_saves = 0;
    int num_global = 0;
    int num_heap = 0;
    int num_stack = 0;
    
    while (fscanf(fp, "%c,%lx\n", &operation, &address) != EOF) {
        if (operation == 'I') {
            num_instructions++;
        } else if (operation == 'M') {
            num_modifies++;
        } else if (operation == 'L') {
            num_loads++;
        } else if (operation == 'S') {
            num_saves++;
        }

        if (operation != 'I') {
            if (address >= GLOBALS_START && address <= GLOBALS_END) {
                num_global++;
            } else if (address >= HEAP_START && address <= HEAP_END) {
                num_heap++;
            } else if (address >= STACK_START) {
                num_stack++;
            }
        }
    }
    fclose(fp);

    /* Use these print statements to print the ouput. It is important that 
     * the output match precisely for testing purposes.
     * Fill in the relevant variables in each print statement.
     * The print statements are commented out so that the program compiles.  
     * Uncomment them as you get each piece working.
     */
    /*
    printf("Reference Counts by Type:\n");
    printf("    Instructions: %d\n", );
    printf("    Modifications: %d\n", );
    printf("    Loads: %d\n", );
    printf("    Stores: %d\n", );
    printf("Data Reference Counts by Location:\n");
    printf("    Globals: %d\n", );
    printf("    Heap: %d\n", );
    printf("    Stack: %d\n", );
    */

    printf("Reference Counts by Type:\n");
    printf("    Instructions: %d\n", num_instructions);
    printf("    Modifications: %d\n", num_modifies);
    printf("    Loads: %d\n", num_loads);
    printf("    Stores: %d\n", num_saves);
    printf("Data Reference Counts by Location:\n");
    printf("    Globals: %d\n", num_global);
    printf("    Heap: %d\n", num_heap);
    printf("    Stack: %d\n", num_stack);

    return 0;
}
