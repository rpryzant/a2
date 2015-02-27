// hi tony

/*
 * An implementation of the Williams Shell.
 * (c) 2015 Tony Liu and Reid Pryzant.
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

int main(int argc, char **argv) {

  char buf[BUFSIZ];
  //move these back down into loop 
  int args_i;
  char *args[BUFSIZ];
  while(buf == fgets(buf, BUFSIZ, stdin)) {
    
    char *cur = buz;
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
  }
  int i;
  for(i = 0; i < args_i; i++) {
    printf("%s\n", args[i]);
    free(args[i]);
  }
  
  return 0;
}
