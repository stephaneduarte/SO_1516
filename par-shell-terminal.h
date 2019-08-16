#ifndef PAR_SHELL_TERMINAL_H
#define PAR_SHELL_TERMINAL_H

/*Function to deal with SIGINT and SIGUSR*/
void signalDeath(int signo);

/*Will wait for a valid input (and check for errors)*/
void waitArguments(char** vector);

#endif