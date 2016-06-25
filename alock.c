/*
 * Copyright (C) 2016 Julien Bonjean <julien@bonjean.info>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <sys/stat.h>

int debug_enabled = 0;

void debug(const char *format, ...) {
	if(!debug_enabled)
		return;
	va_list args;
	va_start(args, format);
	vprintf(format, args);
	va_end(args);
}

// djb2.
// http://www.cse.yorku.ca/~oz/hash.html
unsigned long hash(const char *str)
{
	unsigned long hash = 5381;
	int c;

	while ((c = *str++))
		hash = ((hash << 5) + hash) + c;

	return hash;
}

int get_cmdline(pid_t pid, char **cmdline) {
	char path[256];
	FILE *proc = NULL;
	size_t len = 0;
	int size = 0;

	// open /proc/xx/cmdline.
	sprintf(path, "/proc/%d/cmdline", pid);

	debug("opening %s\n", path);
	proc = fopen(path, "r");
	if (proc == NULL)
		return -1;

	// read the cmdline.
	debug("reading cmdline\n");
	size = getline(cmdline, &len, proc);
	fclose(proc);

	return size;
}

int extract_script_path(const char *cmdline, int cmdline_size, char **script_path) {
	// ensure there is more than one '\0' delimited string.
	if (cmdline_size <= strlen(cmdline) + 1)
		return 1;

	// first part is the interpreter, so we skip to the second part.
	*script_path = strndup(cmdline + strlen(cmdline) + 1, -1);
	debug("calling script path: %s\n", *script_path);
	return 0;
}

int check_lock(const char *lock_file_path) {
	char buffer[100];
	struct stat st;
	int read = 0;
	char exist_cmdline[255];
	FILE *lock_file = NULL;

	lock_file = fopen(lock_file_path, "r");
	if (lock_file == NULL)
		return 0;

	// try to get the PID from the lock file.
	read = fread(buffer, 1, sizeof(buffer), lock_file);
	fclose(lock_file);
	if (read <= 0)
		return 0;
	debug("read pid %s from lock file %s\n", buffer, lock_file_path);

	// found a PID, check if the process still exists.
	sprintf(exist_cmdline, "/proc/%s/cmdline", buffer);
	debug("checking %s\n", exist_cmdline);
	return !stat(exist_cmdline, &st);
}

int write_lock(const char *lock_file_path, pid_t pid) {
	char buffer[100];
	int written = 0;
	FILE *lock_file = NULL;

	lock_file = fopen(lock_file_path, "w+");
	if (lock_file == NULL)
		return 1;

	// write ppid to lock file.
	sprintf(buffer, "%d", pid);
	debug("writing pid %d to lock file %s\n", pid, lock_file_path);
	written = fwrite(buffer, 1, strlen(buffer) + 1, lock_file);
	fclose(lock_file);
	if (written == strlen(buffer) + 1)
		return 0;
	return 1;
}

int main(int argc, char **argv) {
	char *cmdline = NULL;
	int cmdline_size = 0;
	char *script_path = NULL;
	char lock_file_path[256];

	if (argc > 1 && !strcmp(argv[1], "-d"))
		debug_enabled = 1;

	pid_t ppid = getppid();
	debug("ppid: %d\n", ppid);

	// retrieve cmdline from /proc.
	cmdline_size = get_cmdline(ppid, &cmdline);
	if (cmdline_size == -1) {
		fprintf(stderr, "could not read cmdline\n");
		if (cmdline != NULL)
			free(cmdline);
		return 1;
	}

	// extract script path from cmdline.
	if (extract_script_path(cmdline, cmdline_size, &script_path)) {
		fprintf(stderr, "could not extract script path\n");
		free(cmdline);
		return 1;
	}
	free(cmdline);


	// create the lockfile path.
	sprintf(lock_file_path, "/tmp/alock-%lu.lock", hash(script_path));
	free(script_path);

	// check if it is already locked.
	if (check_lock(lock_file_path))
		return 1;

	// finally acquire the lock.
	if (write_lock(lock_file_path, ppid)) {
		fprintf(stderr, "could not write lock file\n");
		return 1;
	}

	return 0;
}

