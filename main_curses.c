#include <ncurses.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <regex.h>


/* Std start to main, argc holds the number of arguments provided to the executable, and *argv[] an 
   array of strings/chars with the arguments (as strings). 
*/
int main(int argc, char *argv[]){

  int maxlines,maxcols;
  /*
  initscr();
  cbreak();
  noecho();
  */
  
  maxlines = LINES - 1;
  maxcols = COLS - 1;
  
  int wLine,wCol;
  wLine=0;
  wCol=0;

  


  /* GET name and port from arguments. */
 
  char *hoststring,*portstring, *rest, *org;
  org=strdup(argv[1]);
  rest=argv[1];
  hoststring=strtok_r(rest,":",&rest);
  portstring=strtok_r(rest,":",&rest);
  //  printf("Got %s split into %s and %s \n",org, hoststring,portstring);
  mvprintw(wLine,wCol,"Got %s split into %s and %s \n",org, hoststring,portstring);
  wLine++;


  /* This is to test nicknames */
  char *expression="^[A-Za-z_]+$";
  regex_t regularexpression;
  int reti;
  
  reti=regcomp(&regularexpression, expression,REG_EXTENDED);
  if(reti){
    fprintf(stderr, "Could not compile regex.\n");
    exit(1);
  }
  
  int matches;
  regmatch_t items;
  
  printf("Testing nicknames. \n");
  
  for(int i=2;i<argc;i++){
    printf("Testing : |%s|\n", argv[i]);
    if(strlen(argv[i])<12){
      reti=regexec(&regularexpression, argv[i],matches,&items,0);
      if(!reti){
	printf("Nick %s is accepted.\n",argv[i]);
	//	mvprintw(wLine,wCol,"Nick is accepted.",argv[i]);
      } else {
	printf("%s is not accepted.\n",argv[i]);
	//	mvprintw(wLine,wCol,"Nick is not accepted.",argv[i]);
      }
    } else {
      printf("%s is too long (%d vs 12 chars).\n", argv[i], strlen(argv[i]));
      //      mvprintw(wLine,wCol,"%s is too long (%d vs 12 chars).\n", argv[i], strlen(argv[i]));
    }
    wLine++;
  }
  printf("Leaving\n");
  regfree(&regularexpression);
  free(org);





  return(0);
  
}
