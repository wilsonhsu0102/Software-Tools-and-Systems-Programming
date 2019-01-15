#include <stdio.h>

int main(int argc, char **argv) {
	char phone[11];
	int integer;
	scanf("%s", phone);
	scanf("%d", &integer);

	if (integer == -1) {
		printf("%s", phone);
	}
	else if (0 <= integer && integer <= 9) {
		printf("%c", phone[integer]);
	}
	else {
		printf("ERROR");
		return 1;
	}
	return 0;
}