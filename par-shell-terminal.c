#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>

#include "commandlinereader.h"
#include "pipe-io.h"
#include "par-shell-terminal.h"

#define MAX_ARGUMENTS 5
#define MAX_PATH_SIZE 20
#define MAX_PID_SIZE 9
#define MAX_STAT_SIZE 100
#define MAX_DIRPATH_SIZE 100

/*Needs to be global for the interruption treatment*/
int outPipeDesc;

int main(int argc, char* argv[]){
    char* vector[MAX_ARGUMENTS + 2];
    char pipePath[MAX_DIRPATH_SIZE + 1 + MAX_PID_SIZE];
    char pipeName[11+MAX_PID_SIZE];
    char tempBuffer[MAX_STAT_SIZE];
    int inPipeDesc;

    /*-------------------------------------------*/
    /*-------------------INIT--------------------*/
    /*-------------------------------------------*/
    sprintf(pipeName, "/terminal-in-%d", getpid());
    getcwd(pipePath, MAX_DIRPATH_SIZE);
    strcat(pipePath, pipeName);


    if(argc != 2){
        fprintf(stderr, "Incorrect argument specification\nUsage <pipe_name>\n");
        return EXIT_FAILURE;
    }
    if ((outPipeDesc = open("par-shell-in", O_WRONLY)) == -1){
        fprintf(stderr, "Couldn't open pipe/incorrect pipe path\n");
        return EXIT_FAILURE;
    }

    /*---[ Signal Handling ]---*/
    signal(SIGINT, signalDeath);
    signal(SIGUSR1, signalDeath);

    OpenCloseConnection(outPipeDesc, CONNECTION);

    /*-------------------------------------------*/
    /*------------MAIN FUNCTIONALITY-------------*/
    /*-------------------------------------------*/
    while(1){
        waitArguments(vector);
        
        /*---[ Check for the local exit command ]---*/
        if(!strcmp(vector[0], "exit"))
            break;

        /*---[ Check for the stats command ]---*/
        if(!strcmp(vector[0], "stats")){
            vector[1] = pipePath;
            if( mkfifo(pipePath, 0666) < 0 ){
                perror("Error creating fifo pipe");
                return EXIT_FAILURE;
            }
            vectorToPipe(outPipeDesc, vector);        
            if( (inPipeDesc = open(pipePath, O_RDONLY)) == -1){
                fprintf(stderr, "Error opening fifo pipe\n");
                return EXIT_FAILURE;
            }
            printf("---Statistics---\n");
            readFromPipe(inPipeDesc, tempBuffer);
            printf("Number of Children Running: %s\n", tempBuffer);
            readFromPipe(inPipeDesc, tempBuffer);
            printf("Total Running Time: %ss\n", tempBuffer);
            close(inPipeDesc);
            unlink(pipePath);
            continue;
        }

        /*---[ Propagate all other commands ]---*/
        vectorToPipe(outPipeDesc, vector);

        /*---[ After propagating, if it was global exit, exits ]---*/
        if(!strcmp(vector[0], "exit-global"))
            break;

        free(vector[0]);
    }

    /*-------------------------------------------*/
    /*------------PROGRAM TERMINATION------------*/
    /*-------------------------------------------*/
    free(vector[0]);
    OpenCloseConnection(outPipeDesc, DISCONNECTION);
    close(outPipeDesc);
    return EXIT_SUCCESS;
}



/*Will wait for a valid input (and check for errors)*/
void waitArguments(char** vector){
    int err;
    /*The function readLineArguments will wait for input*/
    while( (err = readLineArguments(vector, MAX_ARGUMENTS + 2)) == -1 || err == 0)
        if(err == -1)
            /*If there was an error reading the arguments, will issue it and wait for another command*/
            fprintf(stderr, "Error: Couldn't read the command\n");
}

/*Function to deal with SIGINT and SIGUSR*/
void signalDeath(int signo){
    /*If it was the SIGINT, will send a disconect message*/
    if(signo==SIGINT) OpenCloseConnection(outPipeDesc, DISCONNECTION);
    exit(EXIT_FAILURE);
}