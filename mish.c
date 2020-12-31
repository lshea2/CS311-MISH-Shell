#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

int main() {
	
	setbuf(stdout,0);
	setbuf(stderr,0);
	
	//say starting up
	printf("Starting up.\n");
	
	//Variables for getline()
	char *line_buf = NULL;
	size_t line_buf_size = 0;
	//size_t line_size;
	
	//Variables for strsep()
	char *line_buf_token = NULL;
	char *line_buf_end = NULL;
	
	//Variables for fork/exec process
	pid_t pid;
	int status;
	int ret;
	char *execCommand = NULL;
	char *execParameters[1024];
	
	//Variabls for cd
	char cwd[1024];
	char prevDir[1024];
	char tempDir[1024];
	prevDir[0] = '\0';
	tempDir[0] = '\0';
	
	//Variable to track exit command
	int exitStatus = 0;
	
	//Variable to track special commands
	int normCommand = 0;
	
	//Variable to track log status
	FILE *logFile = NULL;
	int logStatus = -1; // -1 = no log files open, 0 = log file open
	int fileCloseStatus = -1;
	
	while(1) { //while NOT exit
		
		//Shell prompt
		printf("MISH>"); 
		fflush(stdout);
		
		normCommand = 0;
		
		//Read command line using stdin
		if(getline(&line_buf, &line_buf_size, stdin) == -1) {
			exitStatus = 1;
			break;
		}
		
		//log commands if log file is open
		if(logStatus == 0) {
			fprintf(logFile, "%s", line_buf);
		}
		
		//Tokenize Command line
		line_buf_end = line_buf;
		while((line_buf_token = strsep(&line_buf_end, " \t\v\f\n\r")) != NULL) {
			if(*line_buf_token == '\0') {
				continue;
			}
			
			//Special conditions that skip fork
			//exit SHELL condition
			if(strcmp(line_buf_token,"exit") == 0) {
				normCommand = 1;
				exitStatus = 1;
				break;
			}
			//cd SHELL condition
			if(strcmp(line_buf_token,"cd") == 0) {
				normCommand = 1;
				getcwd(cwd,sizeof(cwd));
				strcpy(tempDir, prevDir);
				strcpy(prevDir, cwd);
				line_buf_token = strsep(&line_buf_end, " \t\v\f\n\r");
				if(chdir(line_buf_token) == 0) {
					continue;
				}
				else if(*line_buf_token == '\0') {
					if(chdir(getenv("HOME")) == 0) {
						continue;
					}
					else {
						printf("Error: No directory exists with that path\n");
						if(logStatus == 0) {
							fprintf(logFile, "Error: No directory exists with that path\n");
						}
						continue;
					}
				}
				else if(*line_buf_token == '-') {
					if(tempDir[0] == '\0') {
						perror("There is no previous directory, it has not been switched yet.\n");
						if(logStatus == 0) {
							fprintf(logFile, "There is no previous directory, it has not been switched yet.\n");
						}
						continue;
					}
					else {
						if(chdir(tempDir) == 0) {
							continue;
						}
						else {
							printf("Error: No directory exists with that path\n");
							if(logStatus == 0) {
								fprintf(logFile, "Error: No directory exists with that path\n");
							}
							continue;
						}
					}
				}
				else {
					printf("Error: No directory exists with that path\n");
					if(logStatus == 0) {
						fprintf(logFile, "Error: No directory exists with that path\n");
					}
					continue;
				}
			}
			//pwd SHELL condition
			if(strcmp(line_buf_token,"pwd") == 0) {
				normCommand = 1;
				getcwd(cwd,sizeof(cwd));
				printf("%s\n", cwd);
				continue;
			}
			//log filename and log SHELL conditions
			if(strcmp(line_buf_token,"log") == 0) {
				normCommand = 1;
				line_buf_token = strsep(&line_buf_end, " \t\v\f\n\r");
				if(*line_buf_token == '\0') {
					if(logStatus == -1) {
						printf("There is no current log open, enter log <file name> to open a log file.\n");
					}
					else if(logStatus == 0) {
						//close open file
						fileCloseStatus = fclose(logFile);
						if(fileCloseStatus != 0) {
							perror("File failed to close.\n");
							logStatus = 0;
						}
						else {
							logStatus = -1;
						}
					}
				}
				else {
					if(logStatus == -1) {
						//open file
						logFile = fopen(line_buf_token, "w");
						//dup2(fileno(logFile),fileno(stderr));
						if(logFile == NULL) {
							perror("Error opening file.\n");
							logStatus = -1;
						}
						else {
							logStatus = 0;
						}
					}
					else if(logStatus == 0) {
						//close previous file open new file
						fileCloseStatus = fclose(logFile);
						if(fileCloseStatus != 0) {
							perror("File failed to close, so new file was not opened.\n");
							logStatus = 0;
							continue;
						}
						logFile = fopen(line_buf_token, "w");
						//dup2(fileno(logFile),fileno(stderr));
						if(logFile == NULL) {
							perror("Error opening file.\n");
							logStatus = -1;
						}
						else {
							logStatus = 0;
						}
					}
				}
				continue;
			}
			
			//empty SHELL condition
			if(strcmp(line_buf,"\n") == 0) {
				continue;
			}
			
			//Call fork to start new process, call exec in new process
			if(normCommand == 1) {
				continue;
			}
			
			execCommand = line_buf_token;
			execParameters[0] = line_buf_token;
			int j = 1;
			while((line_buf_token = strsep(&line_buf_end, " \t\v\f\n\r")) != NULL) {
				if(*line_buf_token == '\0') {
					continue;
				}
				execParameters[j] = line_buf_token;
				j++;
			}
			
			execParameters[j] = NULL;
			
			pid = fork();
		
			if (pid < 0) {
				perror("Parent: fork failed:");
				if(logStatus == 0) {
					fprintf(logFile, "Error: fork failed\n");
				}
				exit(1);
			}
	
			if (pid == 0) { // Child executes this block
				int execrc = execvp(execCommand,execParameters);
				if (execrc==-1) {
					perror("Child exec error:");
					if(logStatus == 0) {
						fprintf(logFile, "Error: child exec error\n");
					}
					exit(99);
				}
			}
		
			if (pid > 0) { //Parent executes this block
				ret = waitpid(pid, &status, 0);
				if (ret < 0) {
					perror("Parent: waitpid failed:");
					if(logStatus == 0) {
						fprintf(logFile, "Error: waitpid failed\n");
					}
					exit(2);
				}
				
				if(logStatus == 0) {
					if(status != 0) {
						fprintf(logFile, "Error in child: exited with status %d\n", WEXITSTATUS(status));
					}
				}
			}
			
		}
		
		if(exitStatus == 1) {
			if(logStatus == 0) {
				//close file
				fileCloseStatus = fclose(logFile);
				if(fileCloseStatus != 0) {
					perror("File failed to close.\n");
					logStatus = 0;
				}
				else {
					logStatus = -1;
				}
			}
			break;
		}
		
		
		
	}
	
	//free line buffer
	free(line_buf);
	line_buf = NULL;
	
	//say goodbye
	printf("Goodbye.\n");
	
	return 0;
}