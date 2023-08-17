#include <stdio.h>
#include <sys/types.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <iostream>
#include <sys/wait.h>
#include <cstring>
#include <csignal>
#include <vector>
using namespace std;

#define MAX_INPUT_SIZE 1024
#define MAX_TOKEN_SIZE 64
#define MAX_NUM_TOKENS 64
#define MAX_NUM_PROCESSES 64

/* Splits the string by space and returns the array of tokens
 *
 */

int foreground_process_group = -1;
int background_process_group = -1;
int serial_foreground_cancelled = false;
// Signal Handler to handle termination of foreground process
void handleSigint(int signal_no)
{

	kill(-foreground_process_group, signal_no);
	serial_foreground_cancelled = true;
	cout << endl;
}

// Tokenize function to tokenize the input statement
char **tokenize(char *line)
{
	char **tokens = (char **)malloc(MAX_NUM_TOKENS * sizeof(char *));
	char *token = (char *)malloc(MAX_TOKEN_SIZE * sizeof(char));
	int i, tokenIndex = 0, tokenNo = 0;

	for (i = 0; i < strlen(line); i++)
	{

		char readChar = line[i];

		if (readChar == ' ' || readChar == '\n' || readChar == '\t')
		{
			token[tokenIndex] = '\0';
			if (tokenIndex != 0)
			{
				tokens[tokenNo] = (char *)malloc(MAX_TOKEN_SIZE * sizeof(char));
				strcpy(tokens[tokenNo++], token);
				tokenIndex = 0;
			}
		}
		else
		{
			token[tokenIndex++] = readChar;
		}
	}

	free(token);
	tokens[tokenNo] = NULL;
	return tokens;
}

int main(int argc, char *argv[])
{
	char line[MAX_INPUT_SIZE];
	char **tokens;
	int i;

	// Defining my signal ctrl+C and giving it a custom signal handler
	signal(SIGINT, handleSigint);

	setpgid(getpid(), getpid());

	while (1)
	{

		// Cleaning any zombie child processes
		int cleaning_status = 1;
		while (cleaning_status != -1 && cleaning_status != 0)
		{
			cleaning_status = waitpid(-1, &cleaning_status, WNOHANG);
			if (cleaning_status > 0)
			{
				cout << "Shell: Background process finished " << endl;
			}
		}

		// Checking if there is a request for running multiple foreground process
		int foreground_serial = false;
		int foreground_parallel = false;
		serial_foreground_cancelled = false;
		foreground_process_group = -1;

		/* BEGIN: TAKING INPUT */
		bzero(line, sizeof(line));
		cout << "$ " << flush;
		scanf("%[^\n]", line);
		getchar();

		// printf("Command entered: %s (remove this debug output later)\n", line);
		/* END: TAKING INPUT */

		line[strlen(line)] = '\n'; // terminate with new line
		tokens = tokenize(line);

		// do whatever you want with the commands, here we just print them

		// for(i=0;tokens[i]!=NULL;i++){
		// 	printf("found token %s (remove this debug output later)\n", tokens[i]);
		// }

		for (int i = 0; tokens[i] != NULL; i++)
		{

			if (strcmp(tokens[i], "&&") == 0)
			{
				foreground_serial = true;
				break;
			}

			else if (strcmp(tokens[i], "&&&") == 0)
			{
				foreground_parallel = true;
				break;
			}
		}

		// Creating an array of all commands
		char ***commands = NULL;
		commands = (char ***)malloc(MAX_NUM_PROCESSES * sizeof(char **));
		int commandNo = 0;
		for (int i = 0; tokens[i] != NULL; i++)
		{
			char **command = (char **)malloc(MAX_NUM_TOKENS * sizeof(char *));
			int tokenNo = 0;
			while (tokens[i] != NULL && strcmp(tokens[i], "&&") != 0 && strcmp(tokens[i], "&&&") != 0)
			{
				command[tokenNo] = (char *)malloc(MAX_TOKEN_SIZE * sizeof(char));
				strcpy(command[tokenNo++], tokens[i++]);
			}
			command[tokenNo] = NULL;
			commands[commandNo++] = command;
			if (tokens[i] == NULL)
				break;
		}

		commands[commandNo] = NULL;


		//To ensure we are cleaning tokens afterwards 
		char **tokentoFree = tokens;

		for (int cno = 0; commands[cno] != NULL; cno++)
		{
			if (serial_foreground_cancelled)
			{
				break;
			}
			tokens = commands[cno];
			// Check to avoid any segmentation faults
			if (tokens[0] != NULL)
			{

				// Killing all process background and my shell if exit is called
				if (strcmp("exit", tokens[0]) == 0)
				{
					kill(-background_process_group, SIGKILL);
					cleaning_status = 1;
					while (cleaning_status != -1 && cleaning_status != 0)
					{
						cleaning_status = waitpid(-1, &cleaning_status, WNOHANG);
					}
					kill(getpid(), SIGKILL);
				}

				// Handling cd command
				if (strcmp("cd", tokens[0]) == 0)
				{

					// Handling if too many arguements given cd
					if (tokens[2] != NULL)
					{
						cout << "Shell: Too many arguements" << endl;
						continue;
					}
					int chdir_rv = chdir(tokens[1]);

					// chdir return -1 if there is an err with chdir
					if (chdir_rv != 0)
					{
						cout << "Shell: Incorrect command" << endl;
					}
				}

				// Handling any other command
				else
				{
					// Checking if & is present for backfground execution
					bool background_process = false;
					for (int i = 0; tokens[i] != NULL; i++)
					{
						if (strcmp(tokens[i], "&") == 0 && tokens[i + 1] == NULL)
						{
							tokens[i] = NULL;
							background_process = true;
							break;
						}
					}

					// Forking
					int c_pid = fork();

					// Error in forking
					if (c_pid < 0)
					{
						cout << "Error creating a child process" << endl;
					}
					// Child process
					else if (c_pid == 0)
					{

						int exec_return = execvp(tokens[0], tokens);
						if (exec_return != 0)
						{
							cout << tokens[0] << ": command not found" << endl;
							return EXIT_FAILURE;
						}
					}
					// Parent process
					else
					{
						int status;
						if (background_process)
						{
							if (background_process_group < 0)
							{
								background_process_group = c_pid;
							}

							setpgid(c_pid, background_process_group);
							waitpid(c_pid, &status, WNOHANG);
						}
						else
						{
							if (foreground_process_group < 0)
							{
								foreground_process_group = c_pid;
							}

							int set_pgid_return_value = setpgid(c_pid, foreground_process_group);

							if (set_pgid_return_value == -1)
							{
								foreground_process_group = c_pid;
								setpgid(c_pid, foreground_process_group);
							}

							if (foreground_parallel)
							{
								waitpid(-foreground_process_group, NULL, WNOHANG);
							}

							else
							{
								waitpid(c_pid, &status, 0);
							}
						}
					}
				}
			}
		}

		for (int i = 0; i < commandNo; i++)
		{
			waitpid(-foreground_process_group, NULL, 0);
		}

		// Freeing the created commands array
		for (int i = 0; commands[i] != NULL; i++)
		{
			for (int j = 0; commands[i][j] != NULL; j++)
			{
				free(commands[i][j]);
			}

			free(commands[i]);
		}
		free(commands);

		// Freeing the allocated memory
		for (i = 0; tokentoFree[i] != NULL; i++)
		{
			free(tokentoFree[i]);
		}
		free(tokentoFree);
	}
	return 0;
}
