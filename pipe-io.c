#include "pipe-io.h"

#include <signal.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>

/*------------------------------------------------------------
    Will send a connection or disconnection message to pipe
-------------------------------------------------------------*/
void OpenCloseConnection(int pipeFd, int operation){
    char operationChar;
    char sizeBuffer[MESSAGE_SIZE_BSIZE], pidBuffer[20];
    char* fullMessage ;
    int fullMessageSize;

    if(operation==CONNECTION) operationChar = 'c';
    else if(operation==DISCONNECTION) operationChar = 'd';

    sprintf(pidBuffer, "%ld", (long int)getpid());
    sprintf(sizeBuffer, "%c%04d", operationChar, (int)strlen(pidBuffer) + 1);

    fullMessageSize = strlen(pidBuffer) + 1 + MESSAGE_SIZE_BSIZE;
    fullMessage = (char*) malloc(sizeof(char)*fullMessageSize);

    strcpy(fullMessage, sizeBuffer);
    strcpy(fullMessage + MESSAGE_SIZE_BSIZE, pidBuffer);
    if(write(pipeFd, fullMessage, fullMessageSize) < 0){
        perror("Error creating comunnication");
        exit(EXIT_FAILURE);
    }
    free(fullMessage);
}


/*--------------------------------------
    Will read a message from a pipe
----------------------------------------*/
int readFromPipe(int pipeFd, char* receivingBuffer){
    /*---[ Init buffers and variables ]---*/
    char messageSizeBuffer[MESSAGE_SIZE_BSIZE];
    int messageSize;
    int returnValue;

    /*---[ Read the size and check if there are still any pipes connected ]---*/
    if( (returnValue=read(pipeFd, messageSizeBuffer, MESSAGE_SIZE_BSIZE)) <= 0){
        if(returnValue==0){
            /*Means that there is no one connected on the other side of the pipe*/
            return EMPTY_PIPE;
        }
        /*Else its an error*/
        perror("Error reading from pipe");
        exit(EXIT_FAILURE);
    }
    returnValue = NORMAL_MESSAGE;

    /*---[ Checks if the message is actually a connection or disconnection ]---*/
    if(messageSizeBuffer[0] == 'c' || messageSizeBuffer[0] == 'd'){
        returnValue = (messageSizeBuffer[0] == 'c' ? CONNECTION : DISCONNECTION);
        messageSize = atoi(messageSizeBuffer + 1);
    }
    else messageSize = atoi(messageSizeBuffer);

    /*---[ Read the actual message to the receiving buffer ]---*/
    if(read(pipeFd, receivingBuffer, messageSize)<0){
        perror("Error reading from pipe");
        exit(EXIT_FAILURE);
    }
    return returnValue;
}

/*--------------------------------------
    Will send a message to a pipe
---------------------------------------*/
void writeToPipe(int pipeFd, char* message){
    /*---[ Init buffer, variables and message header string ]---*/
    char sizeBuffer[MESSAGE_SIZE_BSIZE];
    char* fullMessage;
    int fullMessageSize;
    sprintf(sizeBuffer, "%05d", (int)strlen(message)+1);

    /*---[ Create full message, in a single buffer (for atomicity) ]---*/
    fullMessageSize = strlen(message) + 1 + MESSAGE_SIZE_BSIZE;
    fullMessage = (char*) malloc(sizeof(char)*fullMessageSize);
    strcpy(fullMessage, sizeBuffer);
    strcpy(fullMessage + MESSAGE_SIZE_BSIZE, message);

    /*---[ Send it over the pipe ]---*/
    if(write(pipeFd, fullMessage, fullMessageSize) < 0){
        perror("Error writing to pipe");
        exit(EXIT_FAILURE);
    }
    free(fullMessage);
}

/*---------------------------------------*/
void vectorToPipe(int pipeFd, char* vector[]){
    char vectorBuffer[TOTAL_VECTOR_MAXSIZE] = "";
    int i;
    for(i = 0; vector[i] != NULL; i++){
        strcat(vectorBuffer, vector[i]);
        strcat(vectorBuffer, "\t");
    }
    writeToPipe(pipeFd, vectorBuffer);
}

/*---------------------------------------*/
int vectorFromPipe(int pipeFd, char* vector[]){
    int readStatus;
    int numtokens = 0;
    char *token;
    char *s ="\n\t";
    char* vectorBuffer = (char*) malloc(sizeof(char)* TOTAL_VECTOR_MAXSIZE);
    
    readStatus = readFromPipe(pipeFd, vectorBuffer);

    /*---[ Check if empty pipe ]---*/
    if(readStatus==EMPTY_PIPE){
        free(vectorBuffer);
        return EMPTY_PIPE;
    }

    /*---[ Check for first time connection]---*/
    if(readStatus==CONNECTION){
        vector[0]=vectorBuffer;
        return CONNECTION;
    }

    /*---[ Check for disconnection]---*/
    if(readStatus==DISCONNECTION){
        vector[0]=vectorBuffer;
        return DISCONNECTION;
    }
    
    /*---[ Get the first token ]---*/
    token = strtok(vectorBuffer, s);

    /*---[ Walk through other tokens ]---*/
    while(token != NULL) {
        vector[numtokens] = token;
        numtokens ++;
        token = strtok(NULL, s);
    }
    vector[numtokens] = NULL;
    return NORMAL_MESSAGE;
}

/*---------------------------------------------------------------------
The main structure for when you need to keep all the connections saved

NOTES: This is a simple linked-list implementation. Only extra functions
will be documented
----------------------------------------------------------------------*/

typedef struct ConnectionNode{
    struct ConnectionNode* next;
    pid_t pid;
} ConnectionNode;

struct pipeConnections_t{
    ConnectionNode* head;
};

PipeConnections newPipeConnections(){
    PipeConnections pipeConn = (PipeConnections) malloc(sizeof(struct pipeConnections_t));
    pipeConn->head = NULL;
    return pipeConn;
}

void addConnection(PipeConnections pipeConn, pid_t pid){
    ConnectionNode* newNode = (ConnectionNode*) malloc(sizeof(ConnectionNode));
    newNode->pid = pid;
    newNode->next = pipeConn->head;
    pipeConn->head = newNode;
}

void removeConnection(PipeConnections pipeConn, pid_t pid){
    ConnectionNode* currentNode, *auxNode;
    if(pipeConn->head->pid==pid){
        auxNode = pipeConn->head->next;
        free(pipeConn->head);
        pipeConn->head = auxNode;
        return;
    }
    for(currentNode = pipeConn->head; currentNode->next != NULL; currentNode = currentNode->next)
        if(currentNode->next->pid==pid){
            auxNode = currentNode->next->next;
            free(currentNode->next);
            currentNode->next = auxNode;
        }

}

/*------------------------------------------------------------
    Will send a specific signal to all the pids in the list
--------------------------------------------------------------*/
void sendSignalToAll(PipeConnections pipeConn, int sign){
    ConnectionNode* currentNode;
    for(currentNode = pipeConn->head; currentNode != NULL; currentNode = currentNode->next){
        kill(currentNode->pid, sign);
    }
}

void destroyPipeConnections(PipeConnections pipeConn){
    ConnectionNode* currentNode, *auxNode;
    for(currentNode = pipeConn->head; currentNode != NULL; currentNode = auxNode){
        auxNode = currentNode->next;
        free(currentNode);
    }
    free(pipeConn);
}