#ifndef PAR_SHELL_H
#define PAR_SHELL_H
#include <stdio.h>

/*The function that handles termination by the INT signal*/
void terminateBySignal(int signum);

/*Function to be uses when creating a new thread*/
void* monitorFunction(void* args);

/*Will read a file and set the global variables corresponding to the iteration and maxtime*/
void readLogFile(FILE* logFile);


#endif