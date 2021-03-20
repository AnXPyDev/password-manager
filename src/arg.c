#include <stdio.h>
#include <string.h>

#include "arg.h"

int argc;
char **argv;

void extract_flag(char *flagname, int *dest) {
	for (int i = 1; i < argc; i++) {
		if (strcmp(flagname, argv[i]) == 0) {
			*dest = 1;
			return;
		}
	}
	return;
}

void extract_arg(char *argname, char **dest) {
	int index = 0;
	for (int i = 1; i < argc; i++) {
		if (strcmp(argname, argv[i]) == 0) {
			index = i;
			break;
		}
	}

	if (index == 0 || index + 1 >= argc) {
		return;
	}

	char *value = argv[index + 1];
	if ('-' == value[0]) {
		return;
	}
	
	*dest = value;
}
