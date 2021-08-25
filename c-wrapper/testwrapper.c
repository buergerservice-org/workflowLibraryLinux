#include <stdio.h> 
#include <stdlib.h>
#include "libwrapper.h"


int main ( int argc, char *argv[] )
{
   const char * c;
   int status=0;
   
   c = wrap_getkeypad();
   printf("output: %s\n", c);
   printf("---------------------------------------------------");
   c = wrap_getcertificate();
   printf("output: %s\n", c);
   printf("---------------------------------------------------");
   status=system("sleep 3");
   c = wrap_startworkflow("009088");
   printf("output: %s\n", c);
} 
