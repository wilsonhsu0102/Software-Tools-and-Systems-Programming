#include <stdio.h>

int main(int argc, char **argv) {
	char phone[11];
	int integer;
	int error = 0;
	scanf("%s", phone);
	
	while (1) {
		scanf("%d", &integer);

		if (integer == -1) {
			printf("%s\n", phone);
		}
		else if (0 <= integer && integer <= 9) {
			printf("%c\n", phone[integer]);
		}
		else {
			printf("ERROR\n");
			error = 1;
		}
	}
	return error;
}