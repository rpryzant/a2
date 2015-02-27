/*
 * An implementation of the Williams Shell.
 * (c) 2015 Tony Liu and Reid Pryzant.
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/*
 * Break a string (buf) into tokens (placed in args)
 */
int tokenizeString(char *buf, char **args) {
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

int main(int argc, char **argv) {
  char buf[BUFSIZ];
  char *args[BUFSIZ];
  int num_tok;
  while(buf == fgets(buf, BUFSIZ, stdin)) {
    num_tok = tokenizeString(buf, args);
  }
  int i;
  for(i = 0; i < num_tok; i++) {
    printf("%s\n", args[i]);
    free(args[i]);
  }
  
  return 0;
}
