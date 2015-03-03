/*
 * An implementation of the Williams Shell.
 * (c) 2015 Tony Liu and Reid Pryzant.
 * 
 * TODO:
 *    -exit [n] => exit with status N
 *    -special characters
 *    -handle special cases of redirection (two carats in a row, etc)
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <dirent.h>
#include <fcntl.h>
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
static int  builtin(token *command, int cmd_len);
static void help(token command);
static void cd(token path);
static void jobs(void);
static void exitWsh(void);
/* SPECIALS */
static int special(token *command, int cmd_len);
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
	error += execute(cmd, cmd_i);
      }
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
  mode_t mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH;
  while(*command) {
    if(!strcmp(*command, "#")) {
      debugPrint("This is a comment.\n");
    }
    else if(!strcmp(*command, "&")) {
      debugPrint("Your process isn't running in the background.\n");
    }
    else if(!strcmp(*command, "|")) {
      debugPrint("PVC Pipe.\n");
    }
    else if(!strcmp(*command, "<")) {
      file_descr = open(*(++command), O_RDONLY);
      src = dup2(file_descr, src);
    }
    else if(!strcmp(*command, ">")) {
      file_descr = creat(*(++command), mode);
      dst = dup2(file_descr, dst);
    }
    else {
      args[args_i] = strdup(*command);
      args_i++;
    }
    command++;
  }
  
  if(args_i && !builtin(args, args_i)) {               
    error += execute(args, args_i);
  }

  //if(dst != STDOUT_FILENO) {
  close(dst);
  dst = dup2(stdout_copy, STDOUT_FILENO);
    // }
  //if(src != STDIN_FILENO) {
    close(src);
    src = dup2(stdin_copy, STDIN_FILENO);
    //}
  
  if(error) {
    return -1;
  }
  return 0;
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
  char *cmd_path;
  pid_t pid = vfork();
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

  printf("%s%s%s$ ", ANSI_GREEN, CUR_PATH, ANSI_RESET);

  parseExecCmd("\n");
  

  while(buf == fgets(buf, BUFSIZ, stdin)) {
    parseExecCmd(buf);
    printf("%s%s%s$ ", ANSI_GREEN, CUR_PATH, ANSI_RESET);
  }
  freeWsh();

  return 0;
}
