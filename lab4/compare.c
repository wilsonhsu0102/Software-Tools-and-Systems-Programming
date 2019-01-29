#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
  Write the main() function of a program that takes exactly two arguments,
  and prints one of the following:
    - "Same\n" if the arguments are the same.
    - "Different\n" if the arguments are different.
    - "Invalid\n" if the program is called with an incorrect number of
      arguments.

  Your main function should return 0, regardless of what is printed.
*/
int main(int argc, char *argv[]) {
  int boolean = 0;
  if (argc != 3) {
    printf("Invalid\n");
  }
  else {
    if (strlen(argv[1]) == strlen(argv[2])) {
      if (strlen(argv[1]) == 0 && strlen(argv[2]) == 0) {
        boolean = 1;
      } else {
        for (int i = 0; i < strlen(argv[1]); i++) {
          if (argv[1][i] == argv[2][i]) {
            boolean = 1;
          } else {
            boolean = 0;
            break;
          }
        } 
      }
    }
    if (boolean == 1) {
      printf("Same\n");
    } else {
      printf("Different\n");
    }
  }
  return 0;
}