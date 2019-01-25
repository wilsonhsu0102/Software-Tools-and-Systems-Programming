#include <stdio.h>
#include <stdlib.h>

void print_state(char *state, int size) {
    for (int i=0; i < size; i++) {
	printf("%c", state[i]);
    }
    printf("\n");
}

void update_state(char *state, int size) {
    char *new_state = malloc(size * sizeof(char));
    if (size > 0) {
	new_state[0] = state[0];
    }
    if (size > 1) {
	new_state[size - 1] = state[size - 1];
	for (int i=1; i < size - 1; i++) {
	    if ((state[i - 1] == '.' && state[i + 1] == '.') || (state[i - 1] == 'X' && state[i + 1] == 'X')) {
		new_state[i] = '.';
	    } else {
		new_state[i] = 'X';
	    }
        }
    }
    for (int i=0; i < size; i++) {
	state[i] = new_state[i];
    }
    free(new_state);
}
