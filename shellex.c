/* $begin shellmain */
#include "csapp.h"
#include<errno.h>
#define MAXARGS   128
#define MAXJOBS	  128

/* Function prototypes */
void eval(char *cmdline);
int parseline(char *buf, char **argv);
int builtin_command(char **argv); 
int change_directory(char**argv);
int pipe_command(char **argv, int *fd_read, int *fd_write);
void sigint_handler();
void sigstp_handler();
void checkchild();
int get_process_id(char **argv);
int run_jobs(char **argv);
int run_kill(char **argv);
int run_fg(char **argv);
int run_bg(char **argv);


typedef struct job{
	pid_t pid;					/* job pid */
	int exist;					/* whether background job exists */
	int state;					/* job state */
	char command[MAXLINE];		/* command line */
	char argv2[MAXLINE];
}job;
job job_list[MAXJOBS];	/* job lists */


int bg_num;		/* number of background jobs */
pid_t fg_pid;	/* foreground process id */
char fp_command[MAXLINE];	/* foreground process command */
char pipe_string[3] = "|\0";


/* SIGINT handler */
void sigint_handler(){
	if(fg_pid == 0)			/* If there is no foreground child, exit */
		exit(0);
	else{
		kill(fg_pid, SIGKILL);					/* else terminate child */
		for(int i = 0; i < bg_num; i++){		/* erase job list */
			if(job_list[i].pid == fg_pid)
				job_list[i].exist = 0;
		}
		fg_pid = 0;
		return;
	}
}

/* SIGSTOP handler */
void sigstp_handler(){
	if(fg_pid == 0){			/* If there is no foreground child, stop */
		kill(getpid(), SIGSTOP);
		return;
	}
	else						/* else stop shild */
		kill(fg_pid, SIGTSTP);

	for(int i = 0; i < bg_num; i++){		/* chage job list status */
		if(!job_list[i].pid == fg_pid){
			job_list[i].state = 0;
			fprintf(stdout, "[%d]	suspended\t\t\t%s", i+1, job_list[i].command);
			fg_pid = 0;
			return;
		}	
	}

	job_list[bg_num].exist = 1;			/* adding job list */
	job_list[bg_num].state = 0;
	job_list[bg_num].pid = fg_pid;
	strcpy(job_list[bg_num].command, fp_command);
	strcpy(job_list[bg_num].argv2, fp_command);
	fprintf(stdout, "[%d]	suspended\t\t\t%s", ++bg_num, fp_command);
	fg_pid = 0;
	return;
}


int main() 
{
    char cmdline[MAXLINE]; /* Command line */
	
	/* 초기화 */
	bg_num = 0;		fg_pid = 0;
	for(int i = 0; i < MAXJOBS; i++){
		job_list[i].exist = 0;
	}
	
	/* Set Signal Handlers */
	Signal(SIGINT, sigint_handler);
	Signal(SIGTSTP, sigstp_handler);

    while (1) {
	/* Read */
	printf("CSE4100-SP-P1> ");                   
	fgets(cmdline, MAXLINE, stdin); 
	if (feof(stdin))
	    exit(0);

	/* Evaluate */
	eval(cmdline);
	checkchild();
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
    char *p_argv[MAXARGS]; /* Argument list pipe_command() */
	int fd[MAXARGS][2];		/* File Descriptor */
	int pipe_num = 0;		
	char errormsg[MAXLINE]; /* String for error message */
	int status;				/* Child process status */
	char argv_cat[MAXARGS]; /* Array for argument cat */

    strcpy(buf, cmdline);
    bg = parseline(buf, argv); 
    if (argv[0] == NULL)  
		return;   /* Ignore empty lines */

	int i=0;	int k;
	while(argv[i] != NULL){
		for(k=0; 1; k++, i++){
			if(argv[i] == NULL){
				p_argv[k] = NULL;
				break;
			}
			else if(!strcmp(argv[i], "|")){		/* read argument until '|' */ 
				p_argv[k] = NULL;
				if(pipe(fd[pipe_num]) < 0)
					exit(-1);
				if(pipe_num == 0)				/* execute first instruction */
					pipe_command(p_argv, NULL, fd[pipe_num]);
				else
					pipe_command(p_argv, fd[pipe_num - 1], fd[pipe_num]);

				pipe_num++;		i++;
				break;
			}
			else
				p_argv[k] = argv[i];
		}
	}
	argv_cat[0] = '\0';
	for(i=0; argv[i] != NULL; i++){
		strcat(argv_cat, argv[i]);
		strcat(argv_cat, " ");
	}	
		strcat(argv_cat, "\n");

	if(pipe_num != 0){
		for(k=0; p_argv[k] != NULL; k++){
			argv[k] = p_argv[k];
		}
		argv[k] = NULL;
		argv[k+1] =  NULL;
	}

    if (!builtin_command(argv)) { //quit -> exit(0), & -> ignore, other -> run
       if((pid=Fork()) == 0){
			if(!strcmp(argv[0], "jobs")){
				char c[MAXLINE];
				for(i=0; i < bg_num; i++){
					if(job_list[i].exist == 1){
						if(job_list[i].state == 1)
							strcpy(c, "running");
						else if(job_list[i].state == 0)
							strcpy(c, "suspended");
						fprintf(stdout, "[%d]	%s\t\t\t%s", i+1, c,job_list[i].command);
					}
				}
				exit(0);			
			}
			if(pipe_num !=0){
				Close(fd[pipe_num -1][1]);
				
				if(fd[pipe_num -1][0] != 0){
					Dup2(fd[pipe_num - 1][0], 0);
					Close(fd[pipe_num - 1][0]);
				}
			}
			strcpy(errormsg, argv[0]);

			if (execvpe(argv[0], argv, environ) < 0) {	//ex) /bin/ls ls -al &
            	fprintf(stderr, "%s: %s\n", errormsg, strerror(errno));
            	exit(0);
        	}	
			exit(0);
	   }

	   else{
			if(pipe_num > 0){
				Close(fd[pipe_num - 1][1]);
				Close(fd[pipe_num - 1][0]);
			}

			/* Parent waits for foreground job to terminate */
			if (!bg){ 
			   fg_pid = pid;
			   strcpy(fp_command, argv_cat);
			   Waitpid(pid, &status, WUNTRACED);
			   checkchild();
			   fg_pid = 0;
			}
			else{	//when there is backgrount process!
				job_list[bg_num].exist = 1;
				job_list[bg_num].pid = pid;
				job_list[bg_num].state = 1;
				strcpy(job_list[bg_num].command, argv_cat);
				strcpy(job_list[bg_num].argv2, argv[0]);
				fprintf(stdout, "[%d] %d\n", ++bg_num, pid);
			}
		}
    }	
	for(i = 0; i < bg_num; i++){		/* check the number of background jobs */
		if(job_list[i].exist == 1)
			return;
	}
	bg_num = 0;
    return;
}

/* If first arg is a builtin command, run it and return true */
int builtin_command(char **argv) 
{
	int status;
    if (!strcmp(argv[0], "quit")) /* quit command */
		exit(0);
	if( !strcmp(argv[0], "exit"))
		exit(0);
    if (!strcmp(argv[0], "&"))    /* Ignore singleton & */
		return 1;
	checkchild();
	if (!strcmp(argv[0], "cd"))
		return change_directory(argv);
	if (!strcmp(argv[0], "kill"))
		return run_kill(argv);
	if (!strcmp(argv[0], "fg"))
		return run_fg(argv);
	if (!strcmp(argv[0], "bg"))
		return run_bg(argv);

    return 0;                     /* Not a builtin command */
}
/* $end eval */

/* $begin parseline */
/* parseline - Parse the command line and build the argv array */
int parseline(char *buf, char **argv) 
{
    char *delim;         /* Points to first space delimiter */
    int argc;            /* Number of args */
    int bg;              /* Background job? */
	int arglen;			/* Length of args */

    buf[strlen(buf)-1] = ' ';  /* Replace trailing '\n' with space */
    while (*buf && (*buf == ' ')) /* Ignore leading spaces */
	buf++;

    /* Build the argv list */
    argc = 0;
    while ((delim = strpbrk(buf, " |"))) {	/* 따옴표와 pipe 띄어쓰기 예외처리 */
		if(*buf == '\"' || *buf == '\''){
			buf = buf + 1;
			delim = strpbrk(buf, "\'\"");
		}
		else if(*delim == '|'){
			if(*buf != '|'){
				argv[argc++] = buf;
			}
			buf = pipe_string;
		}
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

	else{
		arglen = strlen(argv[argc-1]);
		if(argv[argc-1][arglen-1] == '&'){
			argv[argc-1][arglen-1] = '\0';
			bg = 1;
		}	
	}
    return bg;
}
/* $end parseline */

int change_directory(char **argv){
	if(argv[1] == NULL){
		if(chdir(getenv("HOME")))
			fprintf(stderr, "cd: %s\n", strerror(errno));
	}
	else if(argv[2] == NULL){
		if(chdir(argv[1]))
			fprintf(stderr, "cd: %s: %s\n", argv[1], strerror(errno));
	}
	else{
		fprintf(stderr, "USAGE: cd [dir]\n");
	}
	return 1;
}

int pipe_command(char **argv, int *fd_read, int *fd_write){
	int status;
	pid_t pid;
	if((pid = Fork()==0)){
		if(fd_read != NULL){
			Close(fd_read[1]);
			if(fd_read[0] != 0){
				Dup2(fd_read[0], 0);
				Close(fd_read[0]);
			}
		}
		Close(fd_write[0]);
		if(fd_write[1] != 1){
			Dup2(fd_write[1], 1);
			Close(fd_write[1]);
		}
	
		if(execvpe(argv[0], argv, environ) < 0){
			fprintf(stderr, "%s: %s\n", argv[0], strerror(errno));
			exit(0);
		}
		exit(0);
	}
	else{
		if(fd_read != NULL){
			Close(fd_read[0]);
			Close(fd_read[1]);
		}
		Waitpid(pid, &status, WUNTRACED);
	}
	return 1;
}

void checkchild(){
	pid_t pid;
	int status;
	int i;
	for(i = 0; i < bg_num; i++){
		if(!job_list[i].exist)
			continue;
		if(waitpid(job_list[i].pid, &status, WNOHANG != 0)){
			if(WIFSIGNALED(status))
				fprintf(stdout, "[%d] Terminated		%s", i+1, job_list[i].command);
			else
				fprintf(stdout, "[%d] Done			%s", i+1, job_list[i].command);
			job_list[i].exist = 0;
		}
	}
	return;
}

/* string to process number */
int get_process_id(char **argv){
	int pid = 0;

	if(argv[1] == NULL){
		fprintf(stderr, "%s: need an argument\n", argv[0]);
		return -1;
	}
	if(argv[1][0] != '%'){		/* process number starts with % */
		fprintf(stderr, "%s: %s cannot find such job\n", argv[0], argv[1]);
		return -1;
	}
	for(int i = 1; argv[1][i] != '\0'; i++){
		if(argv[1][i] < '0' || argv[1][i] > '9'){
			fprintf(stderr, "%s: %s: cannot find such job\n", argv[0], argv[1]);
			return -1;
		}
		pid *= 10;
		pid += argv[1][i] - '0';
	}

	return pid - 1;
}

int run_kill(char **argv){
	int pid;
	pid = get_process_id(argv);
	
	if(pid < 0)
		return 1;
	if(!job_list[pid].exist){
		fprintf(stderr, "kill: %s: cannot find such job\n", argv[1]);
		return 1;
	}

	if(kill(job_list[pid].pid, SIGINT) < 0)
		fprintf(stderr, "kill: %s\n", strerror(errno));
	job_list[pid].exist = 0;
	return 1;
}

int run_fg(char **argv){
	int pid;
	int status;
	pid = get_process_id(argv);

	if(pid < 0)
		return 1;
	if(!job_list[pid].exist){	/* If such job does not exist */
		fprintf(stderr, "fg: %s: cannot find such job\n", argv[1]);
		return 1;
	}
	
	if(kill(job_list[pid].pid, SIGCONT) < 0)
		fprintf(stderr, "fg: %s\n", strerror(errno));
	fprintf(stdout, "%s\n", job_list[pid].argv2);
	fg_pid = job_list[pid].pid;
	strcpy(fp_command, job_list[pid].argv2);
	strcat(fp_command, "\n\0");
	job_list[pid].state = 1;	/* current job is running */
	Waitpid(job_list[pid].pid, &status, WUNTRACED);
	job_list[pid].exist = 0;
	return 1;
}

int run_bg(char **argv){
	int pid;
	pid = get_process_id(argv);
	if(pid < 0)
		return 1;
	if(!job_list[pid].exist){
		fprintf(stderr, "bg: %s: cannot find such job\n", argv[1]);
		return 1;
	}
	job_list[pid].state = 1;
	if(kill(job_list[pid].pid, SIGCONT) < 0)
		fprintf(stderr, "bg: %s\n", strerror(errno));

	return 1;

}
