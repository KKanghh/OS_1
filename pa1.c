/**********************************************************************
 * Copyright (c) 2021
 *  Sang-Hoon Kim <sanghoonkim@ajou.ac.kr>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTIABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 **********************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <string.h>

#include "types.h"
#include "list_head.h"
#include "parser.h"

/***********************************************************************
 * struct list_head history
 *
 * DESCRIPTION
 *   Use this list_head to store unlimited command history.
 */
LIST_HEAD(history);

struct entry {
	char* string;
	struct list_head list;
};

static int __process_command(char * command);
/***********************************************************************
 * run_command()
 *
 * DESCRIPTION
 *   Implement the specified shell features here using the parsed
 *   command tokens.
 *
 * RETURN VALUE
 *   Return 1 on successful command execution
 *   Return 0 when user inputs "exit"
 *   Return <0 on error
 */
static int run_command(int nr_tokens, char *tokens[])
{
	int pp = false, nr_child = 0, pipefd[2];
	char buffer[MAX_COMMAND_LEN];

	if (strcmp(tokens[0], "exit") == 0) return 0;

	if (strcmp(tokens[0], "cd") == 0) {
		if (nr_tokens == 1 || strcmp(tokens[1], "~") == 0) {
			chdir(getenv("HOME"));
			return 1;
		}
		if (chdir(tokens[1]) == -1) {
			fprintf(stderr, "Unable to change directory to %s\n", tokens[0]);
			return -EINVAL;
		}
		return 1;
	}

	if (strcmp(tokens[0], "history") == 0) {
		struct entry* temp;
		int i = 0;
		list_for_each_entry_reverse(temp, &history, list) {
			fprintf(stderr, "%2d: %s", i++, temp->string);
		}

		return 1;
	}

	if (strcmp(tokens[0], "!") == 0) {
		struct entry* temp = list_last_entry(&history, struct entry, list);
		int num = atoi(tokens[1]);
		for (int i = 0; i < num; i++) {
			temp = list_prev_entry(temp, list);
		}
		char* inst = malloc(sizeof(char) * (strlen(temp->string) + 1));
		strcpy(inst, temp->string);
		__process_command(inst);
		free(inst);
		return 1;
	}

	for (int i = 0; i < nr_tokens; i++) {
		if (strcmp(tokens[i], "|") == 0) {
			pp = true;
			tokens[i] = NULL;
		}
	}

	pid_t pid = fork();

	
	if (pid == 0) {
		if (execvp(tokens[0], tokens) < 0 ) {
			fprintf(stderr, "Unable to execute %s\n", tokens[0]);
			return -EINVAL;
		}
	}

	else {
		int status = 0;

		wait(&status);
		return 1;
	}

	return 1;
	
}





/***********************************************************************
 * append_history()
 *
 * DESCRIPTION
 *   Append @command into the history. The appended command can be later
 *   recalled with "!" built-in command
 */
static void append_history(char * const command)
{
	struct entry* temp = malloc(sizeof(struct entry));
	temp->string = malloc(sizeof(char) * (strlen(command) + 1));
	strcpy(temp->string, command);
	INIT_LIST_HEAD(&temp->list);
	list_add(&temp->list, &history);
	return;
}


/***********************************************************************
 * initialize()
 *
 * DESCRIPTION
 *   Call-back function for your own initialization code. It is OK to
 *   leave blank if you don't need any initialization.
 *
 * RETURN VALUE
 *   Return 0 on successful initialization.
 *   Return other value on error, which leads the program to exit.
 */
static int initialize(int argc, char * const argv[])
{
	return 0;
}


/***********************************************************************
 * finalize()
 *
 * DESCRIPTION
 *   Callback function for finalizing your code. Like @initialize(),
 *   you may leave this function blank.
 */
static void finalize(int argc, char * const argv[])
{
	while (!list_empty(&history)) {
		struct entry* temp = list_first_entry(&history, struct entry, list);
		list_del(&temp->list);
		free(temp->string);
		free(temp);
	}
	return;
}


/*====================================================================*/
/*          ****** DO NOT MODIFY ANYTHING BELOW THIS LINE ******      */
/*          ****** BUT YOU MAY CALL SOME IF YOU WANT TO.. ******      */
static int __process_command(char * command)
{
	char *tokens[MAX_NR_TOKENS] = { NULL };
	int nr_tokens = 0;

	if (parse_command(command, &nr_tokens, tokens) == 0)
		return 1;

	return run_command(nr_tokens, tokens);
}

static bool __verbose = true;
static const char *__color_start = "[0;31;40m";
static const char *__color_end = "[0m";

static void __print_prompt(void)
{
	char *prompt = "$";
	if (!__verbose) return;

	fprintf(stderr, "%s%s%s ", __color_start, prompt, __color_end);
}

/***********************************************************************
 * main() of this program.
 */
int main(int argc, char * const argv[])
{
	char command[MAX_COMMAND_LEN] = { '\0' };
	int ret = 0;
	int opt;

	while ((opt = getopt(argc, argv, "qm")) != -1) {
		switch (opt) {
		case 'q':
			__verbose = false;
			break;
		case 'm':
			__color_start = __color_end = "\0";
			break;
		}
	}

	if ((ret = initialize(argc, argv))) return EXIT_FAILURE;

	/**
	 * Make stdin unbuffered to prevent ghost (buffered) inputs during
	 * abnormal exit after fork()
	 */
	setvbuf(stdin, NULL, _IONBF, 0);

	while (true) {
		__print_prompt();

		if (!fgets(command, sizeof(command), stdin)) break;

		append_history(command);
		ret = __process_command(command);

		if (!ret) break;
	}

	finalize(argc, argv);

	return EXIT_SUCCESS;
}
