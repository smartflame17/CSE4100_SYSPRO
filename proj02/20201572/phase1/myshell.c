/* $begin shellmain */
#include "csapp.h"
#include<errno.h>
#define MAXARGS   128

/* Function prototypes */
void eval(char *cmdline);
int parseline(char *buf, char **argv);
int execute_command(char **argv); 

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
    char *argv[MAXARGS]; /* Argument list execve() */
    char buf[MAXLINE];   /* Holds modified command line */
    int bg;              /* Should the job run in bg or fg? */
    pid_t pid;           /* Process id */
    
    strcpy(buf, cmdline);
    bg = parseline(buf, argv); 
    if (argv[0] == NULL)  
	return;   /* Ignore empty lines */
    if (execute_command(argv)) { //quit -> exit(0), & -> ignore, other -> run
    	printf("%s: Command not found.\n", argv[0]);
        exit(0);
    }

	/* Parent waits for foreground job to terminate */
	if (!bg){ 
	    int status;
	}
	else//when there is backgrount process!
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
/* parseline - Parse the command line and build the argv array */
int parseline(char *buf, char **argv) 
{
    char *delim;         /* Points to first space delimiter */
    int argc;            /* Number of args */
    int bg;              /* Background job? */

    buf[strlen(buf)-1] = ' ';  /* Replace trailing '\n' with space */
    while (*buf && (*buf == ' ')) /* Ignore leading spaces */
	buf++;

    /* Build the argv list */
    argc = 0;
    while ((delim = strchr(buf, ' '))) {
	argv[argc++] = buf;
	*delim = '\0';
	buf = delim + 1;
	while (*buf && (*buf == ' ')) /* Ignore spaces */
            buf++;
    }
    argv[argc] = NULL;
    
    if (argc == 0)  /* Ignore blank line */
	return 1;

    /* Should the job run in the background? */
    if ((bg = (*argv[argc-1] == '&')) != 0)
	argv[--argc] = NULL;

    return bg;
}
/* $end parseline */


