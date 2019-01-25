#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

/* Reads a trace file produced by valgrind and an address marker file produced
 * by the program being traced. Outputs only the memory reference lines in
 * between the two markers
 */

int main(int argc, char **argv) {
    
    if(argc != 3) {
         fprintf(stderr, "Usage: %s tracefile markerfile\n", argv[0]);
         exit(1);
    }

    // Addresses should be stored in unsigned long variables
    unsigned long start_marker, end_marker;
    FILE *marker = fopen(argv[2], "r");
    if (marker) {
        fscanf(marker, "%lx %lx", &start_marker, &end_marker);
        fclose(marker);
    } else {
        printf("Error: Marker file path not found");
    }
    

    /* For printing output, use this exact formatting string where the
     * first conversion is for the type of memory reference, and the second
     * is the address
     */
    // printf("%c,%#lx\n", VARIABLES TO PRINT GO HERE);
    int result;
    char operation;
    unsigned long address;
    int byte;
    FILE *trace = fopen(argv[1], "r");
    if (trace) {
        while ((result = fscanf(trace, "%c %lx,%d\n", &operation, &address, &byte) != EOF) || (result = fscanf(trace, "%c  %lx,%d\n", &operation, &address, &byte) != EOF)) {
            if (address == start_marker) {
                while ((result = fscanf(trace, "%c %lx,%d\n", &operation, &address, &byte) != EOF) || (result = fscanf(trace, "%c  %lx,%d\n", &operation, &address, &byte) != EOF)) {
                    if (address != end_marker) {
                        printf("%c,%#lx\n", operation, address);
                    } else {
                        break;
                    }
                }
            } 
        }
        fclose(trace);
    } else {
        printf("Error: Trace file path not found");
    }
}
