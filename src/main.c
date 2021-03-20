#include <stdlib.h>
#include <stdio.h>
#include <malloc.h>
#include <memory.h>
#include <string.h>
#include <stdarg.h>
#include <unistd.h>
#include <signal.h>


void exitp(int);

#include "arg.h"

char *gpg_cmd = NULL;
char *gpg_args = "\0";
char *command = NULL; char *identity = NULL;
char *working_directory = NULL;
int verbosity = 0;
int cargc = 0;
char *cargv[16];

char *redirect_output = " > /dev/null";

int systemf(char *format, ...) {
	const char *cmd_prefix = "sh -c \"";
	const char *cmd_postfix = "\"";
	const int prefix_len = strlen(cmd_prefix);
	const int postfix_len = strlen(cmd_postfix);
	va_list args;
	va_start(args, format);
	int format_len = vsnprintf(NULL, 0, format, args);
	char *buffer = alloca(prefix_len + format_len + postfix_len + 1);
	memcpy(buffer, cmd_prefix, prefix_len);
	vsprintf(buffer + prefix_len, format, args);
	memcpy(buffer + prefix_len + format_len, cmd_postfix, postfix_len + 1);
	if (verbosity) {
		fprintf(stderr, "system call \"%s\"", buffer);
	}
	int code = system(buffer);
	if (verbosity) {
		fprintf(stderr, " returned code %i\n", code);
	}
	return code;
}

void cmd_help() {
	printf("Help message\n");
}

int check_file_exists(char *path) {
	FILE *file = fopen(path, "r");
	if (file == NULL) {
		return 0;
	}
	fclose(file);
	return 1;
}

int ensure_file_exists(char *path) {
	if (!check_file_exists) {
		FILE *file = fopen(path, "w");
		if (file == NULL) {
			return 0;
		}
		fclose(file);
		return 1;
	}
	return 1;
}

int check_wd_initialized() {
	FILE *config = fopen(".password-manager", "r");
	if (config == NULL) {
		return 0;
	} else {
		fclose(config);
		return 1;
	}
}

void cmd_init() {
	if (check_wd_initialized()) {
		fprintf(stderr, "Repository already initialized\n");
		exitp(1);
	}
init:;
	char *id = identity;
	if (identity == NULL) {
		id = cargv[0];
	}
	if (id == NULL) {
		fprintf(stderr, "No gpg identity provided\n");
		exitp(1);
	}
	FILE *config = fopen(".password-manager", "w");
	if (config == NULL) {
		fprintf(stderr, "Failed to initialize repository\n");
		exitp(1);
	}
	fprintf(config, "%s", id);
	fclose(config);
	fprintf(stderr, "Initialized successfully!\n");
}

int gpg_encrypt_file(char *path, char *dest) {
	return systemf("%s %s -r %s --output %s --encrypt %s %s", gpg_cmd, gpg_args, identity, dest, path, redirect_output);
}

int gpg_decrypt_file(char *path, char *dest) {
	return systemf("%s %s -u %s --output %s --decrypt %s %s", gpg_cmd, gpg_args, identity, dest, path, redirect_output);
}


void lock() {
	if (check_file_exists(".lockfile")) {
		fprintf(stderr, "Error: lockfile does not exist\n");
		exitp(1);
	}
	
	remove("login.unlocked");
	remove("password.unlocked");

	remove(".lockfile");
}

void save() {
	systemf("cp -f login.locked login.locked.backup");
	systemf("cp -f password.locked password.locked.backup");
	
	remove("login.locked");
	remove("password.locked");

	gpg_encrypt_file("login.unlocked", "login.locked");
	gpg_encrypt_file("password.unlocked", "password.locked");
}

int create_empty_file(char *path) {
	FILE *file = fopen(path, "a");
	if (file) {
		fclose(file);
		return 1;
	}
	return 0;
		
}

void unlock() {
	if (check_file_exists(".lockfile")) {
		fprintf(stderr, "Error: lockfile already exists\n");
		exitp(1);
	}
	if (!ensure_file_exists(".lockfile")) {
		fprintf(stderr, "Failed to create lockfile\n");
		exitp(1);
	}
	
	if (check_file_exists("login.locked")) {
		gpg_decrypt_file("login.locked", "login.unlocked");
	} else {
		create_empty_file("login.unlocked");
	}
	
	if (check_file_exists("password.locked")) {
		gpg_decrypt_file("password.locked", "password.unlocked");
	} else {
		create_empty_file("password.unlocked");
	}

}

void cmd_list() {
	unlock();
	systemf("cat login.unlocked");
	lock();
}

void cmd_manual_modify() {
	unlock();
	
	printf("Go ahead and modify the unlocked files. When you're done, press enter.\n");
	fflush(stdout);
	fgetc(stdin);

	save();
	lock();
}

void cmd_get() {
	if (cargc < 0) {
		fprintf(stderr, "No login supplied");
		exitp(1);
	}
	unlock();

	FILE *login = fopen("login.unlocked", "r");
	FILE *password = fopen("password.unlocked", "r");

	if (login == NULL || password == NULL) {
		fprintf(stderr, "Failed to open unlocked files\n");
		exitp(1);
	}

	int size = strlen(cargv[0]) + 1;
	char *buffer = alloca(size);
	buffer[size - 1] = '\0';

	int line_number = 0;
	int found = 0;
	char *it = buffer;
	while (!feof(login)) {
		it = buffer;
		if (strcmp(buffer, cargv[0]) == 0) {
			found = 1;
			break;
		}
		line_number++;
		for (int i = 0; i < size -1; i++) {
			char c = fgetc(login);
			if (c == '\n') {
				goto noskipline;
			}
			*it = c;
			it++;
			if (feof(login)) {
				break;
			}
		}
		while (fgetc(login) != '\n' && !feof(login)) {

		}
noskipline:;
	}

	/*while (buffer != NULL && strcmp(buffer, cargv[0]) != 0) {
		buffer = fgets(buffer, size, login);
		printf("%i: %s\n", line_number, buffer);
		fflush(stdout);
		line_number++;
	}*/

	if (found == 0) {
		printf("password for %s not stored\n", cargv[0]);
		goto end;
	}

	int lastpos = 0;
	for (; line_number > 0; line_number--) {
		lastpos = ftell(password);
		size = 0;
		while (fgetc(password) != '\n' && !feof(password)) {
			size++;
		}
	}
	size++;

	buffer = alloca(size);
	fseek(password, lastpos, SEEK_SET);
	fgets(buffer, size, password);

	printf("%s\n", buffer);


end:;
	fclose(login);
	fclose(password);

	lock();
}
 
void cmd_test() {
	printf("test arguments:\n");
	for (int i = 0; i < cargc; i++) {
		printf(" %i: %s\n", i, cargv[i]);
	}
}


void initialize() {
	// get verbosity level
	{
		char *vb = NULL;
		extract_flag("-v", &verbosity);
		extract_flag("--verbose", &verbosity);
		extract_arg("-v", &vb);
		extract_arg("--verbose", &vb);

		if (vb != NULL) {
			verbosity = atoi(vb);
		}

		if (verbosity) {
			fprintf(stderr, "verbosity set to %i\n", verbosity);
		}
	}

	// get command
	if (argc == 1) {
		fprintf(stderr, "Not enough arguments\n");
		exitp(1);
	}
	if (argv[1][0] != '-') {
		command = argv[1];
	}
	if (command == NULL) {
		fprintf(stderr, "No command name given\n");
		exitp(1);
	}
	

	// get command arguments
	for (int i = 2; i < argc; i++) {
		if (argv[i][0] != '-') {
			cargv[cargc] = argv[i];
			cargc++;
		} else {
			break;
		}
	}

	// get working directory
	extract_arg("-w", &working_directory);
	extract_arg("--working-directory", &working_directory);

	// get gpg identity
	extract_arg("-i", &identity);
	extract_arg("--identity", &identity);

	// get gpg client command
	extract_arg("--gpg-cmd", &gpg_cmd);
	if (gpg_cmd == NULL) {
		if (systemf("which gpg %s", redirect_output) == 0) {
			gpg_cmd = "gpg";
		} else if (systemf("which gpg %s", redirect_output) == 0) {
			gpg_cmd = "gpg";
		} else {
			fprintf(stderr, "Couldn't find a gpg client, check if you have one installed\n");
			exitp(1);
		}
	} else {
		if (systemf("which %s %s", gpg_cmd, redirect_output) != 0) {
			fprintf(stderr, "Provided gpg client not found\n");
		}
	}

	// get additional gpg arguments
	extract_arg("--gpg-args", &gpg_args);
}

void run() {
	if (working_directory != NULL) {
		printf("xx wd %s\n", working_directory); fflush(stdout);
		chdir(working_directory);
		printf("xx wd %s\n", working_directory); fflush(stdout);
	}
	if (!check_wd_initialized()) {
		if (strcmp(command, "test") == 0) {
			cmd_test();
			return;
		}

		if (strcmp(command, "init") == 0) {
			cmd_init();
			return;
		}

		if (strcmp(command, "help") == 0) {
			cmd_help();
			return;
		}
		fprintf(stderr, "Working directory not initialized!\n");
	}
	
	if (identity == NULL) {
		FILE *config = fopen(".password-manager", "r");
		if (config == NULL) {
			fprintf(stderr, "Error: unable to get gpg identity from config file\n");
			exitp(1);
		}
		fseek(config, 0, SEEK_END);
		identity = malloc(ftell(config) + 1);
		rewind(config);
		char *it = identity;
		char c = fgetc(config);
		while (!feof(config)) {
			*it = c;
			it++;
			c = fgetc(config);
		}
		*it = '\0';
	}

	if (strcmp(command, "list") == 0) {
		cmd_list();
		return;
	}

	if (strcmp(command, "manual") == 0) {
		cmd_manual_modify();
		return;
	}

	if (strcmp(command, "get") == 0) {
		cmd_get();
		return;
	}

	if (strcmp(command, "lock") == 0) {
		lock();
		return;
	}
	if (strcmp(command, "unlock") == 0) {
		unlock();
		return;
	}
	if (strcmp(command, "save") == 0) {
		save();
		return;
	}

}

void exitp(int code) {
	remove(".lockfile");
	exit(code);
}

void handle_interrupt(int sig) {
	fprintf(stderr, "\nInterrupt signal received\n");
	exitp(0);	
}


int main(int _argc, char **_argv) { signal(SIGINT, handle_interrupt);
	for (int i = 0; i < 16; i++) {
		cargv[i] = NULL;
	}
	argc = _argc;
	argv = _argv;
	initialize();
	run();
	return 0;
}
