//A *.nix shell meant to be run on top of bash
// Supports the following built-in commands:
// 'exit' - Exits smallsh and kills any processes spawned by smallsh beforehand
// 'cd' - Changes the workind directory of the shell. Defaults to HOME if no argument is supplied
// 'status' - Prints the exit status or terminating signal of the last foreground process
//Author: Shawn McMannis
//CS 344 Operating Systems
//Last mod date: 5/23/2019


#include <dirent.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>


//Global variable to allow/disallow background processes
int allowBG = 1;


//Kills all processes and exits
void ExitShell(int proc[])
{
	int childExitStatus = -5;

	//Kill, and then wait for, all background child processes
	int i;
	for(i = 0; i < 100; i++)
	{
		if(proc[i] > -5)
		{
			kill(proc[i], 15);
			waitpid(proc[i], &childExitStatus, 0);
		}
	}

	//Kill smallsh itself
	exit(0);
}


//Changes the working directory
void ChangeDir(char* params[])
{
	//Temp variables
	char* tempDir;
	char* tempBuffer;
	long tempSize;
	DIR* dir1;
	DIR* dir2;

	//Switch working directory to HOME (no parameters)
	if(params[1] == NULL)
	{
		chdir(getenv("HOME"));
	}
	//Switch working directory to supplied parameter
	else
	{
		tempSize = pathconf(".", _PC_PATH_MAX);
		if((tempBuffer = (char*)malloc((size_t)tempSize)) != NULL)
		{
			//Build directory name
			tempDir = getcwd(tempBuffer, (size_t)tempSize);
			strcat(tempDir, "/");
			strcat(tempDir, params[1]);		

			//Check if directory exists
			dir1 = opendir(tempDir);
			dir2 = opendir(params[1]);
			if(dir1)
			{
				chdir(tempDir);
				closedir(dir1);
				return;
			}
			//Check if directory is an absolute path
			else if(dir2)
			{
				chdir(params[1]);
				closedir(dir2);
				return;
			}
			else
			{
				printf("Directory doesn't exist\n");
				fflush(stdout);
			}
		}
	}
}


//Prints either the exit status or terminating signal of the most recent foreground process
void PrintStatus(int exitStatus, int signalStatus)
{
	//If signalStatus is 0, print exitStatus
	if(!signalStatus)
	{
		printf("exit value %d\n", exitStatus);
		fflush(stdout);
	}
	else
	{
		printf("terminated by signal %d\n", signalStatus);
		fflush(stdout);
	}
}


//SIGTSTP handler
void catchSIGTSTP(int signo)
{
	//Message variables
	char* fgMessage = "\nEntering foreground-only mode (& is now ignored)\n";
	char* bgMessage = "\nExiting foreground-only mode\n";

	//Disable background processes
	if(allowBG == 1)
	{
		allowBG = 0;
		write(STDOUT_FILENO, fgMessage, 51);
	}
	//Enable background processes
	else
	{
		allowBG = 1;
		write(STDOUT_FILENO, bgMessage, 31);
	}
}


//main
void main(int argc, const char* argv[])
{
	//Number of chars entered
	int numCharsEntered = -5;

	//Size of the allocated buffer
	size_t bufferSize = 2048;

	//Points to a buffer allocated by getline() that contains the input
	char* lineEntered = NULL;

	//Points to the command entered
	char* command = NULL;

	//Stores the "#" character
	char comment = '#';

	//Array of pointers used to store entered parameters
	char* params[512];
	int i;
	for(i = 0; i < 512; i++)
	{
		params[i] = NULL;
	}
	
	//Index used to store parameters
	int paramIdx = 0;

	//Delimiter used by strtok
	char* delim = " ";

	//Temporary string used by strtok
	char* tempStr = NULL;

	//Stores the name of the input file, if any
	char* inputFile = NULL;

	//Stores the name of the output file, if any
	char* outputFile = NULL;

	//Flag set if command is to be run in the background
	int bgFlag = 0;

	//Int used to determine length of PID
	int length;

	//Temporarily stores the PID when converting to string
	char* tempPID = NULL;

	//Pointer that stores child exit status
	int childExitMethod = -5;

	//Array that stores the background processes
	int bgProc[100];
	for(i = 0; i < 100; i++)
	{
		bgProc[i] = -5;
	}

	//Index of bgProc
	int bgProcIdx = 0;

	//Stores the result of the background wait call
	int bgResults = -5;

	//Stores the results of the foreground wait call
	int fgResults = -5;

	//Stores the exit/signal status of the background wait call
	int bgStatus = 0;

	//Stores the '$$' string, which should expand to the PID of smallsh
	char* needle = "$$";

	//Pointer that stores the position of the the located 'needle' variable above
	char* needlePtr;

	//Buffer used when expanding '$$'
	static char buffer[2048];

	//Stores the terminating signal of the most recent foregroud process
	int signalStatus = 0;

	//Stores the exit status of the most recent foreground process
	int exitStatus = 0;

	//File descriptors
	int fd0, fd1, fd2, fdi, fdo;

	//Sigaction structs
	struct sigaction SIGINT_action = {0}, SIGTSTP_action = {0}, ignore_action = {0};

	//Set SIGINT action
	SIGINT_action.sa_handler = SIG_DFL;

	//Ignore SIGINT
	ignore_action.sa_handler = SIG_IGN;
	sigaction(SIGINT, &ignore_action, NULL);

	//Set SIGTSTP action
	SIGTSTP_action.sa_handler = catchSIGTSTP;
	sigfillset(&SIGTSTP_action.sa_mask);
	SIGTSTP_action.sa_flags = SA_RESTART;
	sigaction(SIGTSTP, &SIGTSTP_action, NULL);

	
	//Main shell loop
	while(1)
	{
		//Prompt and input loop
		while(1)
		{
			//Check background PIDs for completion
			int k;
			for(k = 0; k < bgProcIdx; k++)
			{
				if(bgProc[k] > -5)
				{
					bgResults = waitpid(bgProc[k], &childExitMethod, WNOHANG);
					//If a background process has finished
					if(bgResults > 0)
					{
						printf("Background PID %d is done: ", bgProc[k]);
						fflush(stdout);
						if(WIFEXITED(childExitMethod))
						{
							bgStatus = WEXITSTATUS(childExitMethod);
							printf("exit value %d\n", bgStatus);
							fflush(stdout);
						}
						else if(WIFSIGNALED(childExitMethod))
						{
							bgStatus = WTERMSIG(childExitMethod);
							printf("terminated by signal %d\n", bgStatus);
							fflush(stdout);
						}

						//Remove the entry from the array
						bgProc[k] = -5;
					}
				}

				//Reset bg variables
				bgResults = -5;
				bgStatus = 0;
				childExitMethod = -5;
			}
			
			write(STDOUT_FILENO, ": ", 3);

			//Get a line of input from the user
			numCharsEntered = getline(&lineEntered, &bufferSize, stdin);

			//Check for interrupt error
			if(numCharsEntered == -1)
				clearerr(stdin);
			else
				break;
		}

		//Remove trailing newline that getline adds
		lineEntered[strcspn(lineEntered, "\n")] = '\0';

		//Get the command from the entered string
		command = strtok(lineEntered, delim);

		//Proceed with analyzing the input if it is not a comment and not NULL
		if((command != NULL) && (command[0] != comment))
		{
			//Copy the command to the parameter array
			params[paramIdx] = calloc(strlen(command), sizeof(char));
			strcpy(params[paramIdx], command);
			paramIdx++;

			//Get the rest of the parameters from the entered string
			do
			{
				tempStr = strtok(NULL, delim);

				//Check tempStr contents
				if(tempStr != NULL)
				{
					//Check for input redirect
					if(strcmp(tempStr, "<") == 0)
					{
						tempStr = strtok(NULL, delim);
						inputFile = calloc(strlen(tempStr), sizeof(char));
						strcpy(inputFile, tempStr);
					}
					//Check for output redirect
					else if(strcmp(tempStr, ">") == 0)
					{
						tempStr = strtok(NULL, delim);
						outputFile = calloc(strlen(tempStr), sizeof(char));
						strcpy(outputFile, tempStr);
					}
					//Check for background character
					else if(strcmp(tempStr, "&") == 0)
					{
						//Check allowBG flag
						if(allowBG == 1)
						{
							bgFlag = 1;
						}
					}
					//Otherwise save the parameter to the params array
					else
					{
						//Expand the '$$' string to the PID
						if(strcmp(tempStr, "$$") == 0)
						{
							length = snprintf(NULL, 0, "%d", getpid());
							tempPID = calloc(length + 1, sizeof(char));
							snprintf(tempPID, length + 1, "%d", getpid());
	
							//Copy the string into the params array
							tempStr = tempPID;
						}
						else if(needlePtr = strstr(tempStr, needle))
						{
							//Convert PID to string
							length = snprintf(NULL, 0, "%d", getpid());
							tempPID = calloc(length + 1, sizeof(char));
							snprintf(tempPID, length + 1, "%d", getpid());
							//Replace the '$$' string with the PID (code from https://www.linuxquestions.org/questions/programming-9/replace-a-substring-with-another-string-in-c-170076/)
							strncpy(buffer, tempStr, needlePtr - tempStr);
							buffer[needlePtr - tempStr] = '\0';
							sprintf(buffer + (needlePtr - tempStr), "%s%s", tempPID, needlePtr + strlen(needle));
							tempStr = buffer;
						}

						//Copy tempStr to params array
						params[paramIdx] = calloc(strlen(tempStr), sizeof(char));
						strcpy(params[paramIdx], tempStr);
						paramIdx++;

						//Clean up memory and reset variables
						if(tempPID)
						{
							free(tempPID);
							tempPID = NULL;
						}
						needlePtr = NULL;
					}
				}
			}while(tempStr != NULL);

			//Process command
			//Exit
			if(strcmp(command, "exit") == 0)
			{
				ExitShell(bgProc);
			}
			//Change directory
			else if(strcmp(command, "cd") == 0)
			{
				ChangeDir(params);
			}
			//Status
			else if(strcmp(command, "status") == 0)
			{
				PrintStatus(exitStatus, signalStatus);
			}
			//Run the command via a fork/exec
			else
			{
				pid_t spawnPid = -5;

				//Fork the shell, then exec the command
				spawnPid = fork();
				switch(spawnPid)
				{
					case 0:
						//Redirect STDERR to /dev/null - otherwise the child will print a duplicate error message if the command is not executable
						fd2 = open("/dev/null", O_WRONLY);
						dup2(fd2, 2);

						//Ignore SIGTSTP signals in all children
						sigaction(SIGTSTP, &ignore_action, NULL);
	
						//Restore default action for SIGINT if process is in foreground
						if(bgFlag == 0)
						{
							sigaction(SIGINT, &SIGINT_action, NULL);
						}

						//Set STDIN/STDOUT to /dev/null/ for background processes
						if(bgFlag == 1)
						{
							fdi = open("/dev/null", O_RDONLY);
							dup2(fdi, 0);

							fdo = open("/dev/null", O_WRONLY);
							dup2(fdo, 1);
						}	

						//Redirect STDIN, if applicable
						if(inputFile)
						{
							fd0 = open(inputFile, O_RDONLY);
							//Error opening file
							if(fd0 < 0)
							{
								printf("Error opening file\n");
								exitStatus = 1;
								signalStatus = 0;
								exit(1);
							}
							//File was opened
							else
							{
								dup2(fd0, 0);
							}
						}
						
						//Redirect STDOUT, if applicable
						if(outputFile)
						{
							fd1 = open(outputFile, O_WRONLY | O_CREAT | O_TRUNC, 0666);
							//Error opening file
							if(fd1 < 0)
							{
								printf("Error opening file\n");
								exitStatus = 1;
								signalStatus = 0;
								exit(1);
							}
							//File was opened
							else
							{
								dup2(fd1, 1);
							}
						}
						//If the exec() failed
						if(execvp(params[0], params) < 0)
						{
							printf("Command not recognized\n");
							fflush(stdout);
							exitStatus = 1;
							signalStatus = 0;
							close(fd0);
							close(fd1);
							close(fd2);
							close(fdi);
							close(fdo);
							//Terminate the child
							exit(1);
						}
						close(fd0);
						close(fd1);
						close(fd2);
						close(fdi);
						close(fdo);
						break;
					default:
						//Foreground process
						if(bgFlag == 0)
						{
							waitpid(spawnPid, &childExitMethod, 0);

							//Get exit or signal status
							if(WIFEXITED(childExitMethod))
							{
								exitStatus = WEXITSTATUS(childExitMethod);
								signalStatus = 0;
							}
							else if(WIFSIGNALED(childExitMethod))
							{
								signalStatus = WTERMSIG(childExitMethod);
				
								//Print signal status immediately if terminated via SIGINT
								if(signalStatus == 2)
								{
									printf("terminated by signal 2\n");
									fflush(stdout);
								}
								exitStatus = 0;
							}
						}
						//Background process
						else
						{
							printf("background PID is %d\n", spawnPid);
							bgProc[bgProcIdx] = spawnPid;
							bgProcIdx++;

							//Reset bgProcIDx is 100 or above (not a good idea, could overwrite processes if number of background processes > 100. Need to correct.)
							if(bgProcIdx >= 100)
							{
								bgProcIdx = 0;
							}
						}
						break;
				}
			}
		}

		//Reset variables and free memory
		lineEntered = NULL;
		int i;
		for(i = 0; i < paramIdx; i++)
		{
			if(params[i])
			{
				free(params[i]);
			}
			params[i] = NULL;
		}
		paramIdx = 0;
		bgFlag = 0;
		if(inputFile)
		{
			free(inputFile);
			inputFile = NULL;
		}
		if(outputFile)
		{
			free(outputFile);
			outputFile = NULL;
		}
	}
}
