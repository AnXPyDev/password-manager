#include <stdlib.h>
#include <stdio.h>
#include <malloc.h>
#include <string.h>
#include <stdarg.h>
#include <unistd.h>


int argc;
char **argv;

struct Argument {
	char *value;
	int status;
};

int check_flag(char *flagname) {
	for (int i = 1; i < argc; i++) {
		if (strcmp(flagname, argv[i]) == 0) {
			return i;
		}
	}
	return 0;
}

void extract_flag(char *flagname, int *dest) {
	*dest = check_flag(flagname);
}

struct Argument get_arg(char *argname) {
	struct Argument ret;
	int index = check_flag(argname);
	if (index == 0) {
		ret.value = NULL;
		ret.status = 1;
		return ret;
	}

	if (index + 1 >= argc) {
		ret.value = NULL;
		ret.status = 2;
	} else {
		ret.value = argv[index + 1];
		if ('-' == ret.value[0]) {
			ret.value = NULL;
			ret.status = 2;
		} else {
			ret.status = 0;
		}
	}
	return ret;
}

void extract_arg(char *argname, char **dest) {
	struct Argument arg = get_arg(argname);
	if (arg.status == 0) {
		*dest = arg.value;
	}
}

int systemf(char *format, ...) {
	va_list args;
	va_start(args, format);
	char *buffer = alloca(vsnprintf(NULL, 0, format, args) + 1);
	vsprintf(buffer, format, args);
	//printf("%s\n", buffer);
	return system(buffer);

}

char *gpg_cmd = NULL;
char *gpg_args = "\0";
char *command = NULL;
char *identity = NULL;
char *working_directory = NULL;
int verbosity = 0;
int cargc = 0;
char *cargv[16];

char *redirect_output = " > /dev/null";

FILE *lockfile = NULL;


FILE *login_unlocked_file = NULL;
FILE *password_unlocked_file = NULL;

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
		exit(1);
	}
init:;
	char *id = identity;
	if (identity == NULL) {
		id = cargv[0];
	}
	if (id == NULL) {
		fprintf(stderr, "No gpg identity provided\n");
		exit(1);
	}
	FILE *config = fopen(".password-manager", "w");
	if (config == NULL) {
		fprintf(stderr, "Failed to initialize repository\n");
		exit(1);
	}
	fprintf(config, "%s", id);
	fclose(config);
}

int gpg_encrypt_file(char *path, char *dest) {
	return systemf("%s %s -r %s --output %s --encrypt %s %s", gpg_cmd, gpg_args, identity, dest, path, redirect_output);
}

int gpg_decrypt_file(char *path, char *dest) {
	return systemf("%s %s -u %s --output %s --decrypt %s %s", gpg_cmd, gpg_args, identity, dest, path, redirect_output);
}



void lock() {
	if (lockfile == NULL) {
		fprintf(stderr, "Error: lockfile is not open\n");
		exit(1);
	}
	
	if (login_unlocked_file != NULL) {
		fclose(login_unlocked_file);
	}

	if (password_unlocked_file != NULL) {
		fclose(password_unlocked_file);
	}
	
	remove("login.unlocked");
	remove("password.unlocked");

	fclose(lockfile);
	remove(".lockfile");
}

void save_modified() {
	systemf("cp -f login.locked login.locked.backup");
	systemf("cp -f password.locked password.locked.backup");
	
	remove("login.locked");
	remove("password.locked");

	gpg_encrypt_file("login.unlocked", "login.locked");
	gpg_encrypt_file("password.unlocked", "password.locked");
}

void unlock() {
	if (lockfile != NULL) {
		fprintf(stderr, "Error: lockfile is already opened somehow\n");
		exit(1);
	}
	lockfile = fopen(".lockfile", "r");
	if (lockfile != NULL) {
		fprintf(stderr, "Error: lockfile is present in working directory\n");
		fclose(lockfile);
		exit(1);
	}
	lockfile = fopen(".lockfile", "w");
	if (lockfile == NULL) {
		fprintf(stderr, "Failed to create lockfile\n");
		exit(1);
	}
	
	if (check_file_exists("login.locked")) {
		gpg_decrypt_file("login.locked", "login.unlocked");
	}
	
	login_unlocked_file = fopen("login.unlocked", "r");


	if (check_file_exists("password.locked")) {
		gpg_decrypt_file("password.locked", "password.unlocked");
	}

	password_unlocked_file = fopen("password.unlocked", "r");
	
}

void cmd_list() {
	unlock();
	systemf("cat login.unlocked");
	lock();
}

void cmd_manual_modify() {
	unlock();
	
	fclose(login_unlocked_file);
	fclose(password_unlocked_file);

	printf("Go ahead and modify the unlocked files. When you're done, press enter.\n");
	fflush(stdout);
	fgetc(stdin);


	login_unlocked_file = fopen("login.unlocked", "r");
	password_unlocked_file = fopen("password.unlocked", "r");
	
	save_modified();
	lock();
}

void cmd_get() {
	if (cargc < 0) {
		fprintf(stderr, "No login supplied");
		exit(1);
	}
	unlock();

	int size = strlen(cargv[0]) + 1;
	char *buffer = alloca(size);
	buffer[size - 1] = '\0';

	int line_number = 0;
	int found = 0;
	char *it = buffer;
	while (!feof(login_unlocked_file)) {
		it = buffer;
		if (strcmp(buffer, cargv[0]) == 0) {
			found = 1;
			break;
		}
		line_number++;
		for (int i = 0; i < size -1; i++) {
			char c = fgetc(login_unlocked_file);
			if (c == '\n') {
				goto noskipline;
			}
			*it = c;
			it++;
			if (feof(login_unlocked_file)) {
				break;
			}
		}
		while (fgetc(login_unlocked_file) != '\n' && !feof(login_unlocked_file)) {

		}
noskipline:;
	}

	/*while (buffer != NULL && strcmp(buffer, cargv[0]) != 0) {
		buffer = fgets(buffer, size, login_unlocked_file);
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
		lastpos = ftell(password_unlocked_file);
		size = 0;
		while (fgetc(password_unlocked_file) != '\n' && !feof(password_unlocked_file)) {
			size++;
		}
	}
	size++;

	buffer = alloca(size);
	fseek(password_unlocked_file, lastpos, SEEK_SET);
	fgets(buffer, size, password_unlocked_file);

	printf("%s\n", buffer);

end:;
	rewind(login_unlocked_file);
	rewind(password_unlocked_file);

	lock();
}
 
void cmd_test() {
	printf("test arguments:\n");
	for (int i = 0; i < cargc; i++) {
		printf(" %i: %s\n", i, cargv[i]);
	}
}


void initialize() {
	// get command
	if (argc == 1) {
		fprintf(stderr, "Not enough arguments\n");
		exit(1);
	}
	if (argv[1][0] != '-') {
		command = argv[1];
	}
	if (command == NULL) {
		fprintf(stderr, "No command name given\n");
		exit(1);
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
			exit(1);
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
		chdir(working_directory);
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
			exit(1);
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

}


int main(int _argc, char **_argv) {
	for (int i = 0; i < 16; i++) {
		cargv[i] = NULL;
	}
	argc = _argc;
	argv = _argv;
	initialize();
	run();

	return 0;
}
