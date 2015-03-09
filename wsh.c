/*
 * An implementation of the Williams Shell.
 * (c) 2015 Tony Liu and Reid Pryzant.
 * 
 * TODO:
 *    -exit [n] => exit with status N
 *    -special characters
 *    -cd needs to modify PATH so that we can execute executables in directories
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <dirent.h>
#include <fcntl.h>
#include <sys/wait.h>

#include "hash.h"
#include "cirq.h"

typedef char* token;
typedef struct job job;

struct job {
  int pid;
  int jid;
  char *name;
};


static int debug = 1;

#define debugPrint if (debug) printf
#define ANSI_GREEN   "\x1b[32m"
#define ANSI_RESET   "\x1b[0m"
#define SPECIAL      0b1
#define BUILTIN      0b10

#define PIPE         2
#define BACKGROUND   1
#define FOREGROUND   0

/* GLOBALS */
static char *CUR_PATH;
static hash EXEC_TABLE;
static cirq JOBS_CIRQ;
static int  JOB_NUM;

static cirq PIPE_CIRQ;

/* UTILITIES */
static void init(void);
static void initKind(void);
static void freeWsh(void);
static int  tokenizeString(char *, token *); //currently not in use
static int  parseExecCmd(char *);
static int  execute(token *command, int cmd_len, int background);
static void  checkJobs(void);

/* BUILTINS */
static int  builtin(token *command, int cmd_len);
static void help(token command);
static void cd(token path);
static void jobs(void);
static void exitWsh(void);
static void killCmd(token job_num);
/* SPECIALS */
static int special(token *command, int cmd_len);
static void checkPipes(void);

/*
 * Break a string (buf) into tokens (placed in args)
 */
int tokenizeString(char *buf, token*args) {
  // pre:  -buf is a valid null-terminated string
  // post: -args is filled with pointers to tokens 
  //       -the number of tokens is returned.
  int args_i;
  char *cur = buf;
  int tok_len = 0;
  args_i = 0;
  while(*cur != '\0') {
    tok_len = strcspn(cur, "\n#;&|<> ");
    if(tok_len) {
      args[args_i] = strndup(cur, tok_len);
      args_i++;
    }
    cur += tok_len;

    if(strspn(cur, ";\n")) {
      //ship off args, args can now be overwritten
      
    }
    else if(strspn(cur, "#&|<>")){
      args[args_i] = strndup(cur, 1);
      args_i++;
    }
    cur++;
  }
  return args_i;
}
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
  token cmd[256] = {0};
  char  tok[1024] = {0};
  int cmd_i = 0;
  int tok_i = 0;
  int error;

  while(*buf == ' ') {buf++;}

  while (*buf != '\0') {
   
    switch(*buf) {
    case ' ':
      if(tok_i) {
	cmd[cmd_i++] = strndup(tok, tok_i);
	tok_i = 0;
      }
      break;
    
    case '#': case '<': case '>': case '&': case '|':
      if(tok_i) 	
	cmd[cmd_i++] = strndup(tok, tok_i);
     
      //push special char
      tok[0] = *buf;
      cmd[cmd_i++] = strndup(tok, 1);
      tok_i = 0;
      flags |= SPECIAL;
      break;
    
    case ';': case '\n':
      if(tok_i)
	cmd[cmd_i++] = strndup(tok, tok_i);

      if (flags&SPECIAL) {
	special(cmd, cmd_i);
      }
      else if(cmd_i && !builtin(cmd, cmd_i)) {               
	error += execute(cmd, cmd_i, FOREGROUND);
      }
      cmd_i = 0;
      tok_i = 0;
      break;
    
    default:
      tok[tok_i++] = *buf;
    }
    buf++;
  }

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
int builtin(token *command, int cmd_len) {
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
    if(cmd_len == 1) help(NULL);
    else             help(command[cur++]);
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
int special(token *command, int cmd_len) {
  token args[256] = {0};
  int args_i = 0;
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
      if(args_i) {
	execute(args, args_i, BACKGROUND);
	args_i = 0;
      }
    }
    else if(!strcmp(*command, "|")) {
      
      pipe(pipefd);
      dup2(pipefd[1], dst);
      
      if(args_i && !builtin(args, args_i))               
	error += execute(args, args_i, PIPE);
      
      close(pipefd[1]);
      dup2(stdout_copy, dst);

      args_i = 0;
      dup2(pipefd[0], src);
      close(pipefd[0]);
    }
    else if(!strcmp(*command, "<")) {
      file_descr = open(*(++command), O_RDONLY);
      dup2(file_descr, src);
      close(file_descr);
    }
    else if(!strcmp(*command, ">")) {
      file_descr = creat(*(++command), mode);
      dup2(file_descr, dst);
      close(file_descr);
    }
    else {
      args[args_i++] = strdup(*command);
    }
    command++;
  }
  
  if(args_i && !builtin(args, args_i)) {               
    error += execute(args, args_i, FOREGROUND);
  }

  close(dst);
  dst = dup2(stdout_copy, STDOUT_FILENO);
  
  close(src);
  src = dup2(stdin_copy, STDIN_FILENO);
  
  //don't return until pipe children are completed
  checkPipes();

  if(error) {
    return -1;
  }
  return 0;
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
 * changes the current working directory to specified path
 */
void cd(token path) {
  if (!path) 
    path = "/";
  if (!chdir(path)) {
    CUR_PATH = getcwd(CUR_PATH, PATH_MAX);
  } 
  else { 
    debugPrint("cd: %s: No such file or directory.\n", path);
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
 * Stops all processes managed by wsh and exits normally.
 */
void exitWsh() {
  int size = cq_size(JOBS_CIRQ);
  job *cur;
  while(size--) {
    cur = (job *)cq_peek(JOBS_CIRQ);
    kill(cur->pid, SIGINT);
    cq_rot(JOBS_CIRQ);
  }
  
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
 * Initializes excecutable commands. 
 */
void initKind() {
  //pre: None
  //post: allocate  a hash table to translate executable -> full path specification                      
  EXEC_TABLE = ht_alloc(997);                                                                          
  // get path:                                                                                        
  char *path = strdup(getenv("PATH"));  /* getenv(3) */
  char name[1024], basename[256];
  char *dirname;
  DIR *dir;
  // split path members                                                                               
  while ((dirname = strsep(&path,":"))) { /* strsep(3) */
    // open directory file                                                                            
    dir = opendir(dirname);               /* opendir(3) */
    if (dir) {
      struct dirent *de;        /* dirent(5) */
      while ((de = readdir(dir))) { /* readdir(3) */
        int type = de->d_type;
        // check regular files....                                                                    
        if (type & DT_REG) {
          strcpy(name,dirname); /* strcpy(3) */
          strcat(name,"/");     /* strcat(3) */
          strcpy(basename,de->d_name);
          strcat(name,basename);
          // ...that are executable                                                                   
          if (0 == access(name,X_OK)) { /* access(2) */
            // add to database if they've not been encountered before                                 
            if (!ht_get(EXEC_TABLE,basename)) {
              // enter into table, but:                                                               
              // make copies of key and value to void poisoning                                       
              ht_put(EXEC_TABLE,strdup(basename),strdup(name)); /* strdup(3) */
            }
          }
        }
      }
    }
  }
}

/*
 * Initializes global vars
 */
void init() {
  CUR_PATH = getcwd(CUR_PATH, PATH_MAX);
  JOBS_CIRQ = cq_alloc();
  PIPE_CIRQ = cq_alloc();
  JOB_NUM = 0;
  initKind();
}

/*
 * Frees all allocated memory associated with Wsh.
 */
void freeWsh() {
  ht_free(EXEC_TABLE);
}

/* 
 * Execute a command, either in background context, foreground context, or pipe context.
 */
int execute(token *command, int cmd_len, int context) {
  //pre: command is a single command & argument list with no special characters.
  //post: the command is executed and status is returned.
  token first = *command;
  char *cmd_path;
  pid_t pid = vfork();
  int status;

  //to tie off end of command
  command[cmd_len] = NULL;

  // child process
  if (pid == 0) {
    cmd_path = ht_get(EXEC_TABLE, first);
    if(cmd_path) {
      if(cmd_len)
	execv(cmd_path, command);
    }
    else {
      fprintf(stdout, "%s: Command not found\n", first);
      exit(-1);
    }
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
    int i;
    int buflen = 0;
    for(i = 0; i < cmd_len; i++) {
      buflen += strlen(command[i])+1;
    }
    char *name = (char *)malloc(buflen);
    for(i = 0; i < cmd_len; i++) {
      strcat(name, command[i]);
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
    parseExecCmd(buf);
    fprintf(stdout, "%s%s%s$ ", ANSI_GREEN, CUR_PATH, ANSI_RESET);
  }
  freeWsh();

  return 0;
}
