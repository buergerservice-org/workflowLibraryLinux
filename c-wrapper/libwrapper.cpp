#include <string>
#include "workflowLibrary.h"
#include "libwrapper.h"


workflowLibrary::workflow w;

const char * wrap_startworkflow(const char * PIN)
{
   std::string o=w.startworkflow(PIN);
   char* c = new char[o.length()+1];
   std::copy(o.begin(), o.end(), c);
   c[o.length()] = '\0';
   return c;
}

const char * wrap_getkeypad( void )
{
   std::string o=w.getkeypad();
   char* c = new char[o.length()+1];
   std::copy(o.begin(), o.end(), c);
   c[o.length()] = '\0';
   return c;
}
 
const char * wrap_getcertificate( void )
{
   std::string o=w.getcertificate();
   char* c = new char[o.length()+1];
   std::copy(o.begin(), o.end(), c);
   c[o.length()] = '\0';
   return c;
}

const char * wrap_readjson(const char * input)
{
   std::string o=w.readjson(input);
   char* c = new char[o.length()+1];
   std::copy(o.begin(), o.end(), c);
   c[o.length()] = '\0';
   return c;
}


