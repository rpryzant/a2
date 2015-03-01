/*
 * An implementation of the Williams Shell.
 * (c) 2015 Tony Liu and Reid Pryzant.
 * 
 * TODO:
 *    -exit [n] => exit with status N
 *    -error handling for builtins and excecute
 *    -write a function that checks if tok is a builtin
 *    -debug builtin
 *    -get builtins to work, need better way to do this (see parseExexCmd)
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

#define debugPrint if (debug) printf
#define ANSI_GREEN   "\x1b[32m"
#define ANSI_RESET   "\x1b[0m"
#define SPECIAL      0b1
#define BUILTIN      0b10


/* GLOBALS */
static char *CUR_PATH;
static hash EXEC_TABLE;
/* UTILITIES */
static void init(void);
static void initKind(void);
static void freeWsh(void);
static int  tokenizeString(char *, token *); //currently not in use
static int  parseExecCmd(char *);
static int  execute(token *command, int cmd_len);
/* BUILTINS */
static int  isBuiltin(token tok);
static int  builtin(token *command, int cmd_len);
static void help(token command);
static void cd(token path);
static void jobs(void);
static void exitWsh(void);

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
  //TODO: repeated builtin check is ugly...how to handle?
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
	
	if(isBuiltin(cmd[cmd_i-1])) 
	  flags |= BUILTIN;

	tok_i = 0;
      }
      break;
    
    case '#': case '<': case '>': case '&': case '|':
      if(tok_i){ 	
	cmd[cmd_i++] = strndup(tok, tok_i);
	
	if(isBuiltin(cmd[cmd_i-1])) 
	  flags |= BUILTIN;	
      }
      //push special char
      tok[0] = *buf;
      cmd[cmd_i++] = strndup(tok, 1);
      tok_i = 0;
      flags |= SPECIAL;
      break;
    
    case ';': case '\n':
      if(tok_i) {
	cmd[cmd_i++] = strndup(tok, tok_i);
	
	if(isBuiltin(cmd[cmd_i-1])) 
	  flags |= BUILTIN;	
      }

      if (flags&SPECIAL)     
	{}//ship to special
      else if(flags&BUILTIN)
	error += builtin(cmd, cmd_i);
      else                  
	error += execute(cmd, cmd_i);
      
      cmd_i = 0;
      tok_i = 0;
      break;
    
    default:
      tok[tok_i++] = *buf;
    }
    buf++;
  }
  if (error) 
    return -1;
  else
    return 0;
}

/** BUILTIN **/

/*
 * Checks whether tok is a builtin 
 */
int isBuiltin(token tok) {
  //pre: tok is a valid token
  //post: returns 1 if tok is a builtin, 0 otherwise
  if(tok) {
    if(!strcmp(tok, "cd")   || 
       !strcmp(tok, "exit") ||
       !strcmp(tok, "help") ||
       !strcmp(tok, "jobs") ||
       !strcmp(tok, "kill")) 
      return 1;
  }
  return 0;
}

/*
 * Executes built-in commands as specified by tokens in command.
 */
int builtin(token *command, int cmd_len) {
  //pre:  command contains valid builtin command
  //post: return 0 command is executed, -1 otherwise
  
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
    debugPrint("Die! Die!\n");
  }
  else {
    debugPrint("first token isn't a builtin\n");
    return -1;
  }
  return 0;
}

/*
 * changes the current working directory to specified path
 */
void cd(token path) {
  if (!chdir(path)) {
    CUR_PATH = getcwd(CUR_PATH, PATH_MAX);
  } 
  else { 
    debugPrint("cd: %s: No such file or directory", path);
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
  //TODO: eventually kill all tasks, or warn users tasks are still running
  exit(0);
}

/*
 * Builtin that lists all current jobs.
 */
void jobs() {
  //TODO
  debugPrint("Tony's unemployed\n");
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
 * Frees all allocated memory associated with Wsh.
 */
void freeWsh() {
  ht_free(EXEC_TABLE);
}

/* 
 * Execute a command.
 */
int execute(token *command, int cmd_len) {
  //pre: command is a single command & argument list with no special characters.
  //post: the command is executed and status is returned.
  token first = *command;
  pid_t pid = vfork();
  char *cmd_path;
  int status;
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
  // parent process
  else {
    waitpid(pid, &status, 0);
  }
  return status;  
}

int main(int argc, char **argv) {
  init();
  char buf[BUFSIZ];
  //char *args[BUFSIZ];
  //int num_tok;
  printf("%s%s%s$ ", ANSI_GREEN, CUR_PATH, ANSI_RESET);
  while(buf == fgets(buf, BUFSIZ, stdin)) {
    parseExecCmd(buf);

    //num_tok = tokenizeString(buf, args);
    // builtin(args, num_tok);
    //execute(args, num_tok);
    printf("%s%s%s$ ", ANSI_GREEN, CUR_PATH, ANSI_RESET);
  }

  /*
  int i;
  for(i = 0; i < num_tok; i++) {
    fprintf(stderr,"%s\n", args[i]);
    free(args[i]);
  }
  */
  freeWsh();

  return 0;
}
