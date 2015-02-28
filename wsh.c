/*
 * An implementation of the Williams Shell.
 * (c) 2015 Tony Liu and Reid Pryzant.
 * 
 * TODO:
 *    -exit [n] => exit with status N
 *    -error handling for builtins and excecute
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/wait.h>

#include "hash.h"

typedef char* token;

static int debug = 1;
#define debugPrint if (debug) fprintf
#define ANSI_GREEN   "\x1b[32m"
#define ANSI_RESET   "\x1b[0m"

/* GLOBALS */
static char *CUR_PATH;
static hash EXEC_TABLE;
/* UTILITIES */
static void init(void);
static void initKind(void);
static int tokenizeString(char *, token *);
static int execute(token *command, int cmd_len);
/* BUILTINS */
static void builtin(token *command, int cmd_len);
static void help(token command);
static void cd(token path);
static void jobs(void);

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
    if(strspn(cur, "#;&|<>")){
      args[args_i] = strndup(cur, 1);
      args_i++;
    }
    cur++;
  }
  return args_i;
}

/*
 * Executes built-in commands as specified by tokens in command.
 */
void builtin(token *command, int cmd_len) {
  //pre:  command contains valid builtin command
  //post: command is executed
  
  //first token is the command itself
  int cur = 0;
  token first = command[cur++];

  if(!strcmp(first, "cd")) {
    cd(command[cur++]);
  }
  else if(!strcmp(first, "exit")) {
    exit(0);
  }
  else if(!strcmp(first, "help")) {
    if(cmd_len == 1) help(NULL);
    else             help(command[cur++]);
  }
  else if(!strcmp(first, "jobs")) {
    debugPrint(stderr, "Tony's unemployed\n");
  }
  else if(!strcmp(first, "kill")) {
    debugPrint(stderr, "Die! Die!\n");
  }
  else {
    debugPrint(stderr, "first token isn't a builtin\n");
  }
}

/*
 * changes the current working directory to specified path
 */
void cd(token path) {
  if (!chdir(path)) {
    CUR_PATH = getcwd(CUR_PATH, PATH_MAX);
  } 
  else { 
    debugPrint(stderr, "cd: %s: No such file or directory", path);
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
      debugPrint(stderr, "cd: cd [dir]\n\n\tChange the shell working directory to DIR.\n\tThe default DIR is the value of the HOME shell variable.\n");
    }
    else if(!strcmp(command, "exit")) {
      debugPrint(stderr, "exit: exit\n\n\tExit the shell.\n");
    }
    else if(!strcmp(command, "help")) {
      debugPrint(stderr, "help: help [command]\n\n\tDisplays brief summaries of builtin commands.\n\tIf command is specified, gives detailed help on commands matching the argument.\n");
    }
    else if(!strcmp(command, "jobs")) {
      debugPrint(stderr, "jobs: jobs \n\n\tLists the active jobs.\n");
    }
    else if(!strcmp(command, "kill")) {
      debugPrint(stderr, "kill: kill [pid] [SIGSPEC|SIGNUM]\n\n\tSend the process identified by PID the signal named by\n\tSIGSPEC or SIGNUM. If neither SIGSPEC or SIGNUM is present, then\n\tSIGTERM is assumed.\n");
    }
    else {
      debugPrint(stderr, "help: no help topics match '%s'.\n", command);
    }
  }
  else{
    printf("These shell commands are defined internally.\nType 'help name' to find out more about the function 'name'.\n\n\t-cd\n\t-exit\n\t-help\n\t-jobs\n\t-kill\n");
  }
}

/*
 * Initializes excecutable commands. 
 */
void initKind() {
  //pre: None
  //post: allocate  a hash table to translate executable -> full path specification                      
  EXEC_TABLE = ht_alloc(997);                                                                           
  // get path:                                                                                        
  char *path = getenv("PATH");  /* getenv(3) */
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
  initKind();
}

/* 
 * Execute a command.
 */
int execute(token *command, int cmd_len) {
  //pre: command is a single command & argument list with no special characters.
  //post: The command has been executed.
  int pid;
  token first = *command;
  command++;
  pid = vfork();
  char *cmd_path;
  // child process
  if (pid == 0) {
    cmd_path = ht_get(EXEC_TABLE, first);
    if(cmd_path) {
      //TODO: error handling
      execv(cmd_path, command);
    }
  } 
  else{
    wait(NULL);
  }
  return 0;
}

int main(int argc, char **argv) {
  init();
  char buf[BUFSIZ];
  char *args[BUFSIZ];
  int num_tok;
  printf("%s%s%s$ ", ANSI_GREEN, CUR_PATH, ANSI_RESET);
  while(buf == fgets(buf, BUFSIZ, stdin)) {
    num_tok = tokenizeString(buf, args);
    builtin(args, num_tok);
    execute(args, num_tok);
    printf("%s%s%s$ ", ANSI_GREEN, CUR_PATH, ANSI_RESET);
  }
  int i;
  for(i = 0; i < num_tok; i++) {
    fprintf(stderr,"%s\n", args[i]);
    free(args[i]);
  }
  
  return 0;
}
