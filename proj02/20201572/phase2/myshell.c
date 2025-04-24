/* $begin shellmain */
#include "csapp.h"
#include<errno.h>

#define MAXARGS   128
#define MAXCMDS   10

/* Function prototypes */
void eval(char *cmdline);
int parseline(char *buf);
int execute_command(char **argv); // executes command in single pipe by creating child process and waiting
int execute_pipeline(void);		// executes pipelines in order, redirecting input and output

typedef struct{			// struct holding each commands within a pipe
	char* argv[MAXARGS];
} Command;

Command commands[MAXCMDS];	// save each commands seperately for piping
int cmd_count;				// # of commands in commands array

int main() 
{
    char cmdline[MAXLINE]; /* Command line */

    while (1) {
	/* Read */
	printf("CSE4100-SP-P2> ");                   
	fgets(cmdline, MAXLINE, stdin); 
	if (feof(stdin))
	    exit(0);

	/* Evaluate */
	eval(cmdline);
    } 
}
/* $end shellmain */
  
/* $begin eval */
/* eval - Evaluate a command line */
void eval(char *cmdline) 
{
    char buf[MAXLINE];   /* Holds modified command line */
    int bg;              /* Should the job run in bg or fg? */
    pid_t pid;           /* Process id */
    
    strcpy(buf, cmdline);
	cmd_count = 0;		// reset cmd_count for parsing
    bg = parseline(buf); 
    if (commands[0].argv[0] == NULL)  
		return;   /* Ignore empty lines */
	
    if (execute_pipeline()) { //quit -> exit(0), & -> ignore, other -> run
    	//printf("%s: Command not found.\n", commands[0].argv[0]);
        //exit(0);
    }
	
	for (int i = 0; i < cmd_count; i++){
		for (int j = 0; commands[i].argv[j] != NULL; j++){
			commands[i].argv[j] = NULL;		//reset pointers for next command
		}
	}
	/* Parent waits for foreground job to terminate */
	if (!bg){ 
	    int status;
	}
	else	//when there is background process!
	    printf("%d %s", pid, cmdline);
    
    return;
}

/* runs command and returns 0 if successful */
int execute_command(char **argv) 
{
	if (!strcmp(argv[0], "cd")){	// command 'cd'
		if (argv[1] == NULL){
			chdir(getenv("HOME"));	// cd with no args goes to home dir
		}
		else if (chdir(argv[1]) != 0) printf("Directory named %s not found.\n", argv[1]);
		return 0;	// command ran successfully
	}
	
	if (!strcmp(argv[0], "echo")){	// command 'echo'
		int i = 1;
		while(argv[i] != NULL){
			printf("%s", argv[i]);
			if (argv[i+1] != NULL) printf(" ");
			i++;
		}
		printf("\n");
		fflush(stdout);
		return 0;
	}
	
    if (!strcmp(argv[0], "quit")) /* quit command */
		exit(0);
	if (!strcmp(argv[0], "exit")) exit(0);	// exit command  
    if (!strcmp(argv[0], "&"))    /* Ignore singleton & */
		return 0;

	// fork and run programs as child process
	pid_t pid = fork();
	if (pid < 0){
		fprintf(stderr, "Fork failed.\n");
		return 1;
	} 
	else if (pid == 0){
		// Child process
		if (execvp(argv[0], argv) == -1){
			fprintf(stderr, "%s: Command not found.\n", argv[0]);
			exit(EXIT_FAILURE);	// abnormal exit
		}
	} 
	else {
		// Parent process
		int status;	//child's exit status
		waitpid(pid, &status, 0);
		if(WIFEXITED(status))
			return 0;
		else {
			fprintf(stderr, "Command %s terminated abnormally.\n", argv[0]);
			return 1;
		}
	}

    return 1;                     /* command failed */
}
/* $end eval */

/* $begin parseline */
/* parseline - Parse the command line and build the commands array */
int parseline(char *buf) 
{
    const char *delim = " \t\n";         /* delimiter for parsing single command */
    int bg;              /* Background job? (Unused in phase2) */
	
	const char* pipe_delim = "|";
	char *cmd_str;
	char *line_ptr = buf;

    buf[strlen(buf)-1] = '\0';  /* Replace trailing '\n' with nullchar */
    while (*buf && (*buf == ' ')) /* Ignore leading spaces */
		buf++;
	
	// build the commands list 
	while ((cmd_str = strtok_r(line_ptr, pipe_delim, &line_ptr)) != NULL && cmd_count < MAXCMDS){
		char *arg_ptr = cmd_str;
		char* token;
		int argc = 0;

		// Trim spaces from single command
		while(*arg_ptr == ' ') arg_ptr++;
		char *end = arg_ptr + strlen(arg_ptr) - 1;
		while(end > arg_ptr && *end == ' ') end--;
		*(end + 1) = '\0';
		
		// Split each args within cmd and build struct Command
		while ((token = strtok_r(arg_ptr, delim, &arg_ptr)) != NULL && argc < MAXARGS - 1){
			// remove quotation marks from arg if needed
			if (*token == '\'' || *token == '\"'){
				char *token_start = token;
				token_start++;
				while (*token_start != '\0' && *token_start != *token) token_start++;
				token++;
				*token_start = '\0';  
			}
			commands[cmd_count].argv[argc++] = token;
		}
		commands[cmd_count].argv[argc] = NULL;		// null terminate argv
		cmd_count++; 
	}

    return 0;	// no bg implementation yet
}
/* $end parseline */

int execute_pipeline(){
	if (cmd_count <= 0) return 0;	// empty line (shouldn't even get here but just in case)
	if (cmd_count == 1) return execute_command(commands[0].argv);	// single command (no pipe)
	
	// -------- Fork / Execute Logic for single pipe -------- 
	int fd[2];				// stores read and write file descriptors for dup syscall
	int input_fd = STDIN_FILENO;	// fd of first command in pipeline (stdin)
	pid_t pid;
	pid_t last_pid = -1;	// pid of last command in pipeline

	for (int i = 0; i < cmd_count; i++){
		// Create pipe for each cmd except last cmd (output is set to stdout)
		if (i < cmd_count - 1){
			pipe(fd);
		}
		pid = fork();
		if (pid < 0){
			fprintf(stderr, "Fork failed.\n");
			if (i < cmd_count - 1){
				close(fd[0]);
				close(fd[1]);
			}
			if (input_fd != STDIN_FILENO) close(input_fd);		// if fork failed mid-pipeline, close input_fd as well
			return 1;
		}
		else if (pid == 0){
			// Child process
			if (input_fd != STDIN_FILENO) {
				dup2(input_fd, STDIN_FILENO);		// redirect input to stdin
				close(input_fd);
			}
			
			if (i < cmd_count - 1){
				dup2(fd[1], STDOUT_FILENO);		// redirect output to stdout
				close(fd[0]);
				close(fd[1]);	
			}
			if (execvp(commands[i].argv[0], commands[i].argv) == -1)
				exit(EXIT_FAILURE);				// should be here if execution failed
		}
		else {
			// Parent process
			if (i < cmd_count - 1) close(fd[1]);	// close write end of current pipe
			if (input_fd != STDIN_FILENO) close(input_fd);	// close read end of previous pipe (if not first cmd)
			if (i < cmd_count - 1) input_fd = fd[0];	// save read end of current pipe for next cmd
			last_pid = pid;		// Update pid of lastest cmd		
		}
	}
	// ---- Parent waits for last command ----
	int status = 0;
	if (last_pid > 0){
		waitpid(last_pid, &status, 0);
		if (cmd_count > 1 && input_fd != STDIN_FILENO) close(input_fd);	// close final read end of last pipe
		if (WIFEXITED(status)) return WEXITSTATUS(status);
		else {
			fprintf(stderr, "Command ternimated abnormally.\n");
			return 1;
		}
	}

}
// TODO: debug line 206 on executing commands sequentially. Especially triple commands
// seems to be problem with grep argument  double quotes are considered part of input string for search, but bash automatically removes these?
