g++ -Wall -c libwrapper.cpp
gcc -Wall -c testwrapper.c
g++ -o testwrapper testwrapper.o libwrapper.o ../workflowLibraryLinux/bin/x64/Release/libworkflowLibraryLinux.a -lssl -lcrypto -lpthread


