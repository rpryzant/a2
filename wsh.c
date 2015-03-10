/*
 * An implementation of the Williams Shell.
 * (c) 2015 Tony Liu and Reid Pryzant.
 * 
 * TODO:
 *    -DEBUGGINGGGGGG
  */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <dirent.h>
#include <fcntl.h>
#include <sys/wait.h>

#include "cirq.h"
#include "vec.h"

typedef char* token;
typedef struct job job;
struct job {
  int pid;
  int jid;
  char *name;
};

//toggles debugPrints on and off
static int debug = 0;

#define debugPrint if (debug) printf
#define ANSI_GREEN   "\x1b[32m"
#define ANSI_RESET   "\x1b[0m"
#define SPECIAL      0b1
#define BUILTIN      0b10

#define PIPE         2
#define BACKGROUND   1
#define FOREGROUND   0

/* GLOBALS */
static int  INT_FLAG; //if interrupt has been sent to us
static char *CUR_PATH;
static cirq JOBS_CIRQ;
static int  JOB_NUM;
static cirq PIPE_CIRQ;

/* UTILITIES */
static void init(void);
static void freeWsh(void);
static int  parseExecCmd(char *);
static int  execute(token *command, int background);
static void checkJobs(void);
static void catchSIGINT(int);

/* BUILTINS */
static int  builtin(token *command);
static void help(token command);
static void cd(token path);
static void jobs(void);
static void exitWsh(void);
static void killCmd(token job_num);

/* SPECIALS */
static int special(token *command);
static void checkPipes(void);

/*
 * Break args into valid commands.
 * Here we should also send appropriate flags to push the commmand to the
 * right place (builtin, execute, or special), possibly returning an int?
 * returns 0 when finished parsing commands.
 */
int parseExecCmd(char *buf) {
  //pre: buf is a null terminated string
  //post: buf is tokenized and commands in buf executed.
  int flags = 0;
  vec cmd = v_alloc();
  char tok[1024] = {0};
  int tok_i = 0;
  int error;

  while(*buf == ' ') {buf++;}

  while (*buf != '\0') {   
    switch(*buf) {
    case ' ':
      if(tok_i) {
	v_add(cmd, strndup(tok, tok_i));
	tok_i = 0;
      }
      break;
    
    case '#': case '<': case '>': case '&': case '|':
      if(tok_i) 	
	v_add(cmd, strndup(tok, tok_i));
     
      //push special char
      tok[0] = *buf;
      v_add(cmd, strndup(tok, 1));
      tok_i = 0;
      flags |= SPECIAL;
      break;
    
    case ';': case '\n':
      if(tok_i)
	v_add(cmd, strndup(tok, tok_i));

      if (flags&SPECIAL) {
	special((token *)v_data(cmd));
      }
      else if(v_size(cmd) && !builtin((token *)v_data(cmd))) {               
	error += execute((token *)v_data(cmd), FOREGROUND);
      }
      v_reset(cmd);
      tok_i = 0;
      break;
    
    default:
      tok[tok_i++] = *buf;
    }
    buf++;
  }

  v_free(cmd);

  if(cq_size(JOBS_CIRQ)){
    checkJobs();
  }

  if (error) 
    return -1;
  else
    return 0;
}


/** BUILTIN **/
/*
 * Executes built-in commands as specified by tokens in command.
 */
int builtin(token *command) {
  //pre:  command contains valid builtin command
  //post: return 1 if command is executed, 0 otherwise
  
  //first token is the command itself
  int cur = 0;
  token first = command[cur++];

  if(!strcmp(first, "cd")) {
    cd(command[cur++]);
  }
  else if(!strcmp(first, "exit")) {
    exitWsh();
  }
  else if(!strcmp(first, "help")) {
    help(command[cur++]);
  }
  else if(!strcmp(first, "jobs")) {
    jobs();
  }
  else if(!strcmp(first, "kill")) {
    killCmd(command[cur++]);
    debugPrint("Die! Die!\n");
  }
  else {
    debugPrint("first token isn't a builtin\n");
    return 0;
  }
  return 1;
}

/** SPECIAL **/
/*
 * Parses all special characters. Perhaps we should ship to helper functions?
 */
int special(token *command) {
  vec args = v_alloc();
  int error = 0;

  int stdin_copy = dup(STDIN_FILENO);
  int stdout_copy = dup(STDOUT_FILENO);
  int src = STDIN_FILENO;
  int dst = STDOUT_FILENO;
  
  int file_descr;  
  int pipefd[2];

  mode_t mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH;

  while(*command) {
    if(!strcmp(*command, "#")) {
      break;
      debugPrint("This is a comment.\n");
    }
    else if(!strcmp(*command, "&")) {
      if(v_size(args)) {
	execute((token *)v_data(args), BACKGROUND);
	v_reset(args);
      }
    }
    else if(!strcmp(*command, "|")) {
      pipe(pipefd);
      dup2(pipefd[1], dst);
      
      if(v_size(args) && !builtin((token *)v_data(args)))               
	 error += execute((token *)v_data(args), PIPE);
      
      close(pipefd[1]);
      dup2(stdout_copy, dst);

      v_reset(args);
      dup2(pipefd[0], src);
      close(pipefd[0]);
    }
    else if(!strcmp(*command, "<")) {
      file_descr = open(*(++command), O_RDONLY);
      if(file_descr == -1) {
	fprintf(stdout, "Wsh: %s: No such file or directory\n", *command);
      }
      else{
	dup2(file_descr, src);
	close(file_descr);
      }
    }
    else if(!strcmp(*command, ">")) {
      file_descr = creat(*(++command), mode);
      dup2(file_descr, dst);
      close(file_descr);
    }
    else {
      v_add(args, strdup(*command));
    }
    command++;
  }
  
  if(v_size(args) && !builtin((token *)v_data(args))) {               
    error += execute((token *)v_data(args), FOREGROUND);
  }

  close(dst);
  dst = dup2(stdout_copy, STDOUT_FILENO);
  
  close(src);
  src = dup2(stdin_copy, STDIN_FILENO);
  
  v_free(args);

  if(error) {
    return -1;
  }
  else {
    //don't return until pipe children are completed
    checkPipes();
    return 0;
  }
}

/*
 * Checks whether all child processes for a pipe command have completed
 */
void checkPipes() {
  //post: returns when all children have completed
  pid_t *cur;

  while(cq_size(PIPE_CIRQ)) {
    cur = (pid_t *)cq_peek(PIPE_CIRQ);
    if(waitpid(*cur, NULL, WNOHANG) == *cur) {
      cq_deq(PIPE_CIRQ);
      free(cur);
    }
    cq_rot(JOBS_CIRQ);
  }
}

/*
 * Catches signals, discards any buffered data, and creates a new prompt
 */
void catchSIGINT(int signo) {
  //debugPrint("User tried to ctl-c! Signal #: %d", signo);
  INT_FLAG = 1;
  fprintf(stdout,"\n%s%s%s$ ", ANSI_GREEN, CUR_PATH, ANSI_RESET);
  fflush(stdout);
}

/*
 * changes the current working directory to specified path
 */
void cd(token path) {
  if (!path) {
    path = getenv("HOME");
  }
  if (!chdir(path)) {
    CUR_PATH = getcwd(CUR_PATH, PATH_MAX);
  } 
  else { 
    fprintf(stdout, "cd: %s: No such file or directory.\n", path);
  }
}

/*
 * Prints help on builtin commands.
 */
void help(token command) {
  //pre: command is a builtin, or NULL
  //post: prints help on command
  if(command) {
    if(!strcmp(command, "cd")) {
      fprintf(stdout, "cd: cd [dir]\n\n\tChange the shell working directory to DIR.\n\tThe default DIR is the value of the HOME shell variable.\n");
    }
    else if(!strcmp(command, "exit")) {
      fprintf(stdout, "exit: exit\n\n\tExit the shell.\n");
    }
    else if(!strcmp(command, "help")) {
      fprintf(stdout, "help: help [command]\n\n\tDisplays brief summaries of builtin commands.\n\tIf command is specified, gives detailed help on commands matching the argument.\n");
    }
    else if(!strcmp(command, "jobs")) {
      fprintf(stdout, "jobs: jobs \n\n\tLists the active jobs.\n");
    }
    else if(!strcmp(command, "kill")) {
      fprintf(stdout, "kill: kill [pid] [SIGSPEC|SIGNUM]\n\n\tSend the process identified by PID the signal named by\n\tSIGSPEC or SIGNUM. If neither SIGSPEC or SIGNUM is present, then\n\tSIGTERM is assumed.\n");
    }
    else {
      fprintf(stdout, "help: no help topics match '%s'.\n", command);
    }
  }
  else{
    fprintf(stdout, "These shell commands are defined internally.\nType 'help name' to find out more about the function 'name'.\n\n\t-cd\n\t-exit\n\t-help\n\t-jobs\n\t-kill\n");
  }
}

/*
 * Stops all processes managed by wsh and exits normally; cleans up all wsh mem used.
 */
void exitWsh() {
  int size = cq_size(JOBS_CIRQ);
  job *cur;
  while(size--) {
    cur = (job *)cq_peek(JOBS_CIRQ);
    kill(cur->pid, SIGINT);
    cq_rot(JOBS_CIRQ);
  }
  freeWsh();
  exit(0);
}

/*
 * Builtin that lists all current jobs.
 */
void jobs() {
  //post: lists all current jobs
  int num_jobs = cq_size(JOBS_CIRQ);
  job *cur;

  while(num_jobs--) {
    cur = (job *)cq_peek(JOBS_CIRQ);
    fprintf(stdout, "[%d] %s%d\n", cur->jid, cur->name, cur->pid);
    cq_rot(JOBS_CIRQ);
  }
}
/*
 * Kills command specified by job_num
 * 
 */
void killCmd(token job_num) {
  //pre: job_num is a valid job number
  //post: kills job_num process, if it exists
  int size = cq_size(JOBS_CIRQ);
  job *cur;
  while(size--) {
    cur = (job *)cq_peek(JOBS_CIRQ);
    if(cur->jid == atoi(job_num)) {
      kill(cur->pid, SIGINT);
      return;
    }
    cq_rot(JOBS_CIRQ);
  }
  
  fprintf(stdout,"No such process\n");
}

/*
 * Initializes global vars
 */
void init() {
  CUR_PATH = getcwd(CUR_PATH, PATH_MAX);
  JOBS_CIRQ = cq_alloc();
  PIPE_CIRQ = cq_alloc();
  JOB_NUM = 0;
  
  if(signal(SIGINT, catchSIGINT) < 0) {
    debugPrint("Error initializing SIGINT handler.\n");
  }
}

/*
 * Frees all allocated memory associated with Wsh.
 */
void freeWsh() {
  cq_free(JOBS_CIRQ);
  cq_free(PIPE_CIRQ);
}

/* 
 * Execute a command, either in background context, foreground context, or pipe context.
 */
int execute(token *command, int context) {
  //pre: command is a single command & argument list with no special characters.
  //post: the command is executed and status is returned.
  token first = *command;
  pid_t pid = vfork();
  int status;

  // child process
  if (pid == 0) {
    execvp(first, command);
    //falls to here if execvp returns with error
    fprintf(stdout, "%s: Command not found\n", first);
    exit(-1);  
  } 
  // parent process, foreground
  else if(context == FOREGROUND){
    waitpid(pid, &status, 0);
  }
  // parent process, background
  else if(context == BACKGROUND){
    job *p = (job *)malloc(sizeof(job));
    p->pid = pid;
    p->jid = JOB_NUM++;
    
    //UGLY
    int i = 0;
    int buflen = 0;
    while(command[i])
      buflen += strlen(command[i++])+1;

    char *name = (char *)malloc(buflen);
    
    i = 0;
    while(command[i]) {
      strcat(name, command[i++]);
      strcat(name, " ");
    }
    p->name = name;

    cq_enq(JOBS_CIRQ, p);
    fprintf(stdout, "[%d] %d\n", p->jid, p->pid);
  }
  else if(context == PIPE) {
    pid_t *p = (pid_t *)malloc(sizeof(pid_t));
    *p = pid;
    cq_enq(PIPE_CIRQ, p);
  }

  return status;  
}
/*
 * Checks JOB_CIRQ for any jobs that have completed.
 */
void checkJobs(void) {
  //post: newly completed jobs, if any, are printed to stdout
  int pid;
  int num_jobs;
  job *cur;
  
  while((num_jobs = cq_size(JOBS_CIRQ)) && (pid = waitpid(-1, NULL, WNOHANG))) {

    while(num_jobs--) {
      cur = (job *)cq_peek(JOBS_CIRQ);
      
      if(cur->pid == pid) {
	fprintf(stdout, "[%d]: finished %s\n", cur->jid, cur->name);

	cq_deq(JOBS_CIRQ);
	free(cur->name);
	free(cur);
	break;
      }
      cq_rot(JOBS_CIRQ);
    }
    
  }
}

int main(int argc, char **argv) {
  init();
  char buf[BUFSIZ];

  fprintf(stdout, "%s%s%s$ ", ANSI_GREEN, CUR_PATH, ANSI_RESET);

  while(buf == fgets(buf, BUFSIZ, stdin)) {
    INT_FLAG = 0;
    parseExecCmd(buf);
    if(!INT_FLAG) {
      fprintf(stdout, "%s%s%s$ ", ANSI_GREEN, CUR_PATH, ANSI_RESET);
    }
  }
  freeWsh();
  exitWsh();
  return 0;
}
