/* $begin shellmain */
#include "csapp.h"
#include<errno.h>

#define MAXARGS   128	// max args per pipe
#define MAXCMDS   10	// max cmds(pipes) per pipeline
#define MAXJOBS   50	// max jobs (running or background) per instance

/* Function prototypes */
void eval(char *cmdline);
int parseline(char *buf);
int execute_command(char *argv); 				// executes command in single pipe by creating child process and waiting
int execute_pipeline(char* argv);				// executes pipelines in order, redirecting input and output

typedef struct{			// struct holding each commands within a pipe
	char* argv[MAXARGS];
} Command;

Command commands[MAXCMDS];	// save each commands seperately for piping
int cmd_count;				// # of commands in commands array

/* Phase3 Implemented structs, enums, global vars */
typedef enum{
	FOREGROUND,
	BACKGROUND,
	RUNNING,
	STOPPED,
	TERMINATED
} JobStatus;

typedef struct{
	int job_id;				// ID of job (current index on job_list)
	pid_t pid;				// Actual job pid
	char cmd[MAXLINE];		// cmd line
	JobStatus state;		// state of job (RUNNING, STOPPED, TERMINATED)
	JobStatus bg;			// whether job is in foreground or background
	int notified;			// whether user was informed of job being TERMINATED (checking before removing it from job list)
} Job;

Job* job_list[MAXJOBS];		//stores pointers to dynamically allocated job structs
int job_cnt = 0;			// # of current jobs

sigset_t mask, prev;		// for blocking, unblocking signals

////////////////////////////////////////////////////

/* Phase3 Implmentated function prototypes */
void sigchld_handler(int sig);	// waits for job to end, then deletes it from jobs list
void sigint_handler(int sig);	// kills job and deletes it from jobs list
void sigtstp_handler(int sig);	// stops job and update status accordingly.

void myjobs(Job* jobs[]);	// iterates thru jobs and prints each job name and stats
int myfg(Job* job);			// 'fg' cmd sends SIGCONT to target job, updates job->bg and waits for it to exit
int mybg(Job* job);			// 'bg' cmd sends SIGCONT to target job and updates job->bg
int mykill(Job* job);		// 'kill' cmd sends SIGINT to target job

Job* add_job(pid_t pid, const char* cmd, int bg);	// constructs job struct, and adds it to jobs list
int remove_job(Job* job);							// removes job from jobs list, deallocates memory
Job* find_job_by_id(int job_id);					// returns pointer to job struct by job id
Job* find_job_by_pid(pid_t pid);					// returns pointer to job struct by pid
Job* find_foreground_job(void);						// returns pointer to foreground job
void update_job_list(void);							// helper function to manage job_list (remove killed processes)

int exit_flag = 0;
void cleanup();										// runs when exit cmd is called, deallocating heap memory to prevent mem leaks
//////////////////////////////////////////////


int main() 
{
    char cmdline[MAXLINE]; /* Command line */
	
	// set signal handlers and mask
	Signal(SIGCHLD, sigchld_handler);
	Signal(SIGINT, sigint_handler);
	Signal(SIGTSTP, sigtstp_handler);	
	
	Sigemptyset(&mask);
	Sigaddset(&mask, SIGTSTP);
	Sigaddset(&mask, SIGINT);
	Sigprocmask(SIG_BLOCK, &mask, &prev);	//block SIGTSTP and SIGINT signals so shell itself doesnt get killed by ctrl-c or ctrl-z

    while (!exit_flag) {
	/* Read */
		printf("CSE4100-SP-P2> ");                   
		fgets(cmdline, MAXLINE, stdin); 
		if (feof(stdin))
	    	exit(0);

	/* Evaluate */
		eval(cmdline);
		update_job_list();
    } 
	cleanup();
}
/* $end shellmain */
  
/* $begin eval */
/* eval - Evaluate a command line */
void eval(char *cmdline) 
{
    char buf[MAXLINE];   /* Holds modified command line */
    int bg;              /* Should the job run in bg or fg? */
    pid_t pid;           /* Process id */
    sigset_t mask_all, prev_all;

	Sigfillset(&mask_all);
    strcpy(buf, cmdline);
	cmd_count = 0;		// reset cmd_count for parsing
    //bg = parseline(buf); 
    if (cmdline == "\n")  
		return;   /* Ignore empty lines */
	
	//Sigprocmask(SIG_SETMASK, &mask_all, &prev_all);

	if (!strchr(cmdline, '|')) execute_command(cmdline);	// no pipeline, single cmd
    else execute_pipeline(cmdline); 						 //quit -> exit(0), & -> ignore, other -> run
	
	//Sigprocmask(SIG_SETMASK, &prev_all, NULL);

	for (int i = 0; i < cmd_count; i++){
		for (int j = 0; commands[i].argv[j] != NULL; j++){
			commands[i].argv[j] = NULL;		//reset pointers for next command
		}
	}
    
    return;
}

/* runs command and returns 0 if successful */
int execute_command(char *argv) 
{
	char* original_cmd = malloc(strlen(argv) + 1);
	strcpy(original_cmd, argv);
	int bg = parseline(argv);		// parse given line into commands
	sigset_t mask_all, prev_all;

	Sigfillset(&mask_all);

	if (!strcmp(commands[0].argv[0], "cd")){	// command 'cd'
		if (commands[0].argv[1] == NULL){
			chdir(getenv("HOME"));	// cd with no args goes to home dir
		}
		else if (chdir(commands[0].argv[1]) != 0) printf("Directory named %s not found.\n", commands[0].argv[1]);
		return 0;	// command ran successfully
	}
	
	if (!strcmp(commands[0].argv[0], "echo")){	// command 'echo'
		int i = 1;
		while(commands[0].argv[i] != NULL){
			printf("%s", commands[0].argv[i]);
			if (commands[0].argv[i+1] != NULL) printf(" ");
			i++;
		}
		printf("\n");
		fflush(stdout);
		return 0;
	}
	if (!strcmp(commands[0].argv[0], "jobs")){
		myjobs(job_list);
		return 0;
	}
	int job_id;
	Job* tmp;
	if (!strcmp(commands[0].argv[0], "fg")){
		job_id = atoi(commands[0].argv[1]+1);	//convert %[id] format into integer id
		tmp = find_job_by_id(job_id);
		if (!tmp) printf("Usage : fg %%<job_id>\n");
		else myfg(tmp);
		return 0;
	}
	if (!strcmp(commands[0].argv[0], "bg")){
		job_id = atoi(commands[0].argv[1]+1);	//convert %[id] format into integer id
		tmp = find_job_by_id(job_id);
		if (!tmp) printf("Usage : bg %%<job_id>\n");
		else mybg(tmp);
		return 0;
	}
	if (!strcmp(commands[0].argv[0], "kill")){
		job_id = atoi(commands[0].argv[1]+1);	//convert %[id] format into integer id
		tmp = find_job_by_id(job_id);
		if (!tmp) printf("Usage : kill %%<job_id>\n");
		else mykill(tmp);
		return 0;
	}
	if (!strcmp(commands[0].argv[0], "exit")){
		 exit_flag = 1;	// exit command 
		 return 0;
	} 
    if (!strcmp(commands[0].argv[0], "&"))    /* Ignore singleton & */
		return 0;

	// fork and run programs as child process
	pid_t pid = fork();
	if (pid < 0){
		fprintf(stderr, "Fork failed.\n");
		return 1;
	} 
	else if (pid == 0){
		// Child process
		Sigprocmask(SIG_UNBLOCK, &mask, NULL);	// unblock SIGINT and SIGTSTP so user can ctrl-c or ctrl-z the child process
		if (execvp(commands[0].argv[0], commands[0].argv) == -1){
			fprintf(stderr, "%s: Command not found.\n", commands[0].argv[0]);
			exit(EXIT_FAILURE);	// abnormal exit
		}
	} 
	else {
		// Parent process
		int status;	//child's exit status
		Sigprocmask(SIG_SETMASK, &mask_all, &prev_all); 
		Job* tmp = add_job(pid, original_cmd, bg);
		Sigprocmask(SIG_SETMASK, &prev_all, NULL);
		if (bg) {
			printf("[%d] %d %s\n", tmp->job_id, pid, tmp->cmd);
		}
		else {	// wait for child if it is running on foreground
			sigset_t wait_mask;
			Sigprocmask(0, NULL, &wait_mask);
			Sigdelset(&wait_mask, SIGCHLD);
			while (tmp != NULL && tmp->state == RUNNING)	// wait until job state is not RUNNING (TERMINATED or STOPPED by handler or user)
				Sigsuspend(&wait_mask);
			//waitpid(pid, &status, 0);
		}
		free(original_cmd);		// string copied in add_job, deallocate memory
		return 0;
	}

    return 1;                     /* command failed */
}
/* $end eval */

/* $begin parseline */
/* parseline - Parse the command line and build the commands array */
int parseline(char *buf) 
{
    const char *delim = " \t\n";         /* delimiter for parsing single command */
    int bg = 0;              /* Background job? (Unused in phase2) */
	
	const char* pipe_delim = "|";
	char *cmd_str;
	char *line_ptr = buf;

    buf[strlen(buf)-1] = '\0';  /* Replace trailing '\n' with nullchar */
    while (*buf && (*buf == ' ')) /* Ignore leading spaces */
		buf++;

	char* buf_end = buf + strlen(buf) - 1;
	while (buf_end > buf && *buf_end == ' ') buf_end--;
	if (*buf_end == '&') {
		bg = 1;
		buf_end--;
		while (buf_end > buf && *buf_end == ' ') buf_end--;	
	}	// job is background job
	*(buf_end + 1) = '\0';	//trim trailing spaces and '&' symbol

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

    return bg;
}
/* $end parseline */

int execute_pipeline(char* argv){
	char* original_cmd = malloc(strlen(argv) + 1);
	strcpy(original_cmd, argv);		// copy original command line for job struct as parseline will break it
	int bg = parseline(argv);
	sigset_t mask_all, prev_all;
	Sigfillset(&mask_all);

	if (cmd_count <= 0) return 0;	// empty line (shouldn't even get here but just in case)
	
	// -------- Fork / Execute Logic for single pipe -------- 
	int fd[2];				// stores read and write file descriptors for dup syscall
	int input_fd = STDIN_FILENO;	// fd of first command in pipeline (stdin)
	pid_t pid;
	pid_t last_pid = -1;	// pid of last command in pipeline
	
	Sigprocmask(SIG_SETMASK, &mask_all, &prev_all);
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
	Sigprocmask(SIG_SETMASK, &prev_all, NULL);
	// ---- Parent waits for last command ----
	int status = 0;
	if (last_pid > 0){
		// parent only waits if it is foreground job (!bg)
		if (cmd_count > 1 && input_fd != STDIN_FILENO) close(input_fd);	// close final read end of last pipe
		Sigprocmask(SIG_SETMASK, &mask_all, &prev_all);
		Job* tmp = add_job(last_pid, original_cmd, bg);		// for jobs using pipeline, last cmd in pipe is 'pid' of job
		Sigprocmask(SIG_SETMASK, &prev_all, NULL);
		if (bg){
			printf("[%d] %d %s\n", tmp->job_id, last_pid, tmp->cmd);
		}
		else {	//shell waits if foreground job
			sigset_t wait_mask;
			Sigprocmask(0, NULL, &wait_mask);
			Sigdelset(&wait_mask, SIGCHLD);
			while (tmp != NULL && tmp->state == RUNNING)	// wait until job state is not RUNNING (TERMINATED or STOPPED by handler or user)
				Sigsuspend(&wait_mask);
			//waitpid(last_pid, &status, 0);
		}
		free(original_cmd);
		return 0;
	}
}

// Waits for child to exit, and perform needed actions accordingly (free memory, delete from job list)
void sigchld_handler(int sig){
	// set job state accordingly when waitpid returns
	int saved_errno = errno;
	int status;
	Job* target;
	pid_t pid;
	sigset_t mask_all, prev_all;
	
	Sigfillset(&mask_all);
	while((pid = waitpid(-1, &status, WNOHANG | WUNTRACED)) > 0){
		Sigprocmask(SIG_SETMASK, &mask_all, &prev_all);
		target = find_job_by_pid(pid);
		
		if (target){	// if target is NULL, it is an intermediate cmd in the pipeline, not a job,  so don't handle it
			if (WIFEXITED(status)){							// if exited normally
				if (target->bg == BACKGROUND)
					printf("[%d]\tDone\t\t%s\n", target->job_id, target->cmd);
				target->state = TERMINATED;
				target->notified = 1;						// Informed user job is done, will be deleted by update_job_list 
			}
			else if (WIFSIGNALED(status)){					// if killed by user
				target->state = TERMINATED;
				if (target->bg == FOREGROUND) target->notified = 1;	
			}
			else if (WIFSTOPPED(status)){					// if stopped by user (ctrl-z)
				target->bg = BACKGROUND;
				target->state = STOPPED;
			}
		}
		Sigprocmask(SIG_SETMASK, &prev_all, NULL);
	} 
	errno = saved_errno;
}

// sends SIGINT to foreground job
void sigint_handler(int sig) {
	Job* fg = find_foreground_job();
	if (!fg) {
		fg->state = TERMINATED;
		kill(fg->pid, SIGINT);
	}
}

// sends SIGTSTP to foreground job
void sigtstp_handler(int sig) {
	Job* fg = find_foreground_job();
	if (!fg){
		fg->state = STOPPED;
		kill(fg->pid, SIGTSTP);
	}
}

void myjobs(Job* jobs[]){
	sigset_t mask_all, prev_all;
	Sigfillset(&mask_all);
	Sigprocmask(SIG_SETMASK, &mask_all, &prev_all);
	for (int i = 0; i < job_cnt; i++){
		if (job_list[i]->bg == BACKGROUND){
			printf("[%d]", job_list[i]->job_id);
			switch (job_list[i]->state){
				case RUNNING:
					printf("\tRunning");
					break;
				case STOPPED:
					printf("\tStopped");
					break;
				case TERMINATED:
					printf("\tTerminated");
					break;
			}
			printf("\t\t%s\n", job_list[i]->cmd);
		}
	}
	Sigprocmask(SIG_SETMASK, &prev_all, NULL);
	return;
}

// send SIGCONT to continue job on foreground, and wait for it to finish
int myfg(Job* job){
	int status;

	kill(job->pid, SIGCONT);
	job->state = RUNNING;
	job->bg = FOREGROUND;
	waitpid(job->pid, &status, 0);
}

// set SIGCONT to continue job on background. Don't wait for it.
int mybg(Job* job){
	job->state = RUNNING;
	kill(job->pid, SIGCONT);
}

int mykill(Job* job){
	job->state = TERMINATED;
	kill(job->pid, SIGINT);
}

// Allocates memory for new job and appends it to job_list
Job* add_job(pid_t pid, const char* cmd, int bg){
	if (job_cnt >= MAXJOBS) return NULL;	
	
	Job* newjob = malloc(sizeof(Job));
	newjob->pid = pid;
	if (job_cnt == 0) newjob->job_id = job_cnt+1;		//job id is reset only if job list became empty
	else newjob->job_id = job_list[job_cnt-1]->job_id;	// otherwise job id is {id of lastest job}+1
	strcpy(newjob->cmd, cmd);
	newjob->state = RUNNING;
	newjob->bg = bg;
	newjob->notified = 0;
	
	job_list[job_cnt++] = newjob;

	return newjob;
}

// Frees allocated memory for job, and reduces job_cnt. 
int remove_job(Job* job){
	int idx;	//actual index of target job inside job_list

	if (!job) return 1;
	for (int i = 0; i < job_cnt; i++){
		if (job_list[i]->job_id == job->job_id){
			idx = i;
			free(job);
			break;
		}		
	}
	for (int i = idx; i < job_cnt-1; i++){		 
		job_list[i] = job_list[i + 1];
	}	// scoot up jobs in list
	job_cnt--;	// reduce job_cnt
	return 0;
	
}

Job* find_job_by_id(int job_id){
	//job_id starts from 1
	for (int i = 0; i < job_cnt; i++){
		if (job_list[i]->job_id == job_id)
			return job_list[i];
	}
	return NULL;	// search fail
}

Job* find_job_by_pid(pid_t pid){
	for (int i = 0; i < job_cnt; i++){
		if (job_list[i]->pid == pid)
			return job_list[i];
	}
	return NULL; // search fail
}

Job* find_foreground_job(void){
	for (int i = 0; i < job_cnt; i++){
		if(!job_list[i]->bg) return job_list[i];
	}
	return NULL;	// search fail
}

// removes terminated (whether naturally or by signal) jobs from list
// called after waitpid and macros such as WIFEXITED, or after eval in main loop 
void update_job_list(void){
	sigset_t mask_all, prev_all;
	Sigfillset(&mask_all);
	
	Sigprocmask(SIG_SETMASK, &mask_all, &prev_all);
	
	for (int i = 0; i < job_cnt; i++){
		if (job_list[i]->state == TERMINATED){	//inform user if terminated job was unnotified
			if (!job_list[i]->notified)
				printf("[%d]\tTerminated\t\t%s\n", job_list[i]->job_id, job_list[i]->cmd);
		
			// remove job silently after terminated job has already been notified
			remove_job(job_list[i]);
			i--;
		}		
	}
	Sigprocmask(SIG_SETMASK, &prev_all, NULL);
	return;
}

void cleanup(void){
	sigset_t mask_all, prev_all;
	if (!job_cnt) return;
	
	Sigfillset(&mask_all);
	Sigprocmask(SIG_SETMASK, &mask_all, &prev_all);
 	while (job_cnt){
		remove_job(job_list[job_cnt-1]);
	}	//remove all jobs in list from end of list
	Sigprocmask(SIG_SETMASK, &prev_all, NULL);
	return;
}

//TODO; fix so that cmd line is properly inserted into job list
