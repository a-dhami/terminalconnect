#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <pthread.h>
#include <zconf.h>
#include <netdb.h>
#include "list.h"

#define MAXSIZE 512 //max size of the message that can be sent at once

#define ISDEBUG 0 //set to 1 to enable debug messages

//global variables - pthread
pthread_mutex_t receiverLock;
pthread_mutex_t senderLock;
pthread_cond_t receiverCondition;
pthread_cond_t senderCondition;

pthread_t sender, receiver, input, output;

//global variables - list implementation
LIST *receiverList;
LIST *senderList;

//global variables - UDP implementation
int socketfd; //holds the socket file descriptor
struct sockaddr_in serverAddress, clientAddress;
char *serverPort;
char *remoteMachine;
char *remotePort;

int isRun = 1;

void* socketReceive(void *vargp){
    while(isRun)
    {
        char recvBuffer[MAXSIZE];
        memset(recvBuffer, '\0', MAXSIZE);
        if(ISDEBUG){printf("socketReceiver: waiting for UDP data\n");}
        recvfrom(socketfd, recvBuffer, MAXSIZE, MSG_WAITALL, (struct sockaddr *) &clientAddress,
                 (socklen_t *) sizeof(struct sockaddr_in));
        if(ISDEBUG){printf("socketReceiver: buffer = %s", recvBuffer);}
        if(strlen(recvBuffer) != 0){
            //termination code if '!' received from socket
            if(recvBuffer[0] == '!' && recvBuffer[1] == '\n') {
                isRun = 0;
                pthread_mutex_unlock(&receiverLock);
                pthread_cond_signal(&receiverCondition);
                pthread_cancel(input);
                pthread_cancel(sender);
                pthread_exit(NULL);
            }
            pthread_mutex_lock(&receiverLock);
            int recvLength = strlen(recvBuffer);
            recvBuffer[recvLength] = '\0';
            char *data = malloc(sizeof(char) * MAXSIZE);
            memset(data, '\0', recvLength);
            memcpy(data, recvBuffer, strlen(recvBuffer));
            ListAppend(receiverList, data);
            if(ISDEBUG) {printf("socketReceiver: added item to receiverList\n");
            printf("socketReceiver: items to display: %i\n", ListCount(receiverList));
            printf("socketReceiver: signalling programOutput\n"); }
            pthread_mutex_unlock(&receiverLock);
            pthread_cond_signal(&receiverCondition);

        }
    }
    if(ISDEBUG){printf("socketReceiver: thread closed\n");}
    pthread_exit(NULL);
}

void* socketSend(void *vargp){
    while(isRun)
    {
        pthread_mutex_lock(&senderLock);
        if(ListFirst(senderList) == NULL) {
            if(ISDEBUG){printf("socketSend: waiting for item in senderList.\n");}
            pthread_cond_wait(&senderCondition, &senderLock);
        }
        if(ListFirst(senderList) != NULL){
            if(ISDEBUG){printf("socketSend: message sent\n");}
            char *sendBuffer = (char *) ListRemove(senderList);
            sendto(socketfd, (const char *) sendBuffer, strlen(sendBuffer), 0, (struct sockaddr *) &clientAddress,
                   sizeof(struct sockaddr_in));
            free(sendBuffer);
            pthread_mutex_unlock(&senderLock);
        }
    }
    if(ISDEBUG){printf("socketSend: thread closed\n");}
    pthread_exit(NULL);
}

void* programOutput(void *vargp){

    //GOAL: display the first item in the list and then delete it.
    while(isRun)
    {
        pthread_mutex_lock(&receiverLock);
        if(ListFirst(receiverList) == NULL){
            if(ISDEBUG){printf("programOutput: waiting for List Item\n");}
            pthread_cond_wait(&receiverCondition, &receiverLock);
            if(ISDEBUG){printf("programOutput: received signal from socketReceive\n");}
        }
        if(ListFirst(receiverList) != NULL)
        {
            char *outputBuffer = malloc(sizeof(char) * MAXSIZE);
            outputBuffer = (char *) ListTrim(receiverList);
            if(ISDEBUG){printf("programOutput: receiverList = %s", outputBuffer);
            printf("programOutput: items left to display %i\n", ListCount(receiverList));}
            printf("%s",outputBuffer);
            memset(outputBuffer, '\0', MAXSIZE);
            free(outputBuffer);
            pthread_mutex_unlock(&receiverLock);
        }
    }
    if(ISDEBUG){printf("programOutput: thread closed\n");}
    pthread_exit(NULL);

}

void* programInput(void *vargp){
    while (isRun)
    {
        char *input = malloc(sizeof(char) * MAXSIZE);
        if(ISDEBUG){printf("programInput: waiting for input\n");}
        if(read(STDIN_FILENO, input, MAXSIZE) != 0)
        {
            pthread_mutex_lock(&senderLock);
            char *data = malloc(sizeof(char) * MAXSIZE);
            memcpy(data,input,strlen(input));

            ListAppend(senderList, data);
            if(ISDEBUG){printf("programInput: list appended: %s", data);
            printf("programInput: items to send: %i\n", ListCount(senderList));}

            //termination code for ending chat
            if(input[0] == '!' && input[1] == '\n')
            {
                isRun = 0;
                pthread_mutex_unlock(&senderLock);
                pthread_cond_signal(&senderCondition);
                pthread_cancel(output);
                if(ISDEBUG){printf("programOutput: thread closed\n");}
                pthread_cancel(receiver);
                if(ISDEBUG){printf("socketReceive: thread closed\n");
                printf("programInput: thread closed \n");}
                free(input);
                pthread_exit(NULL);
            }

            memset(input, '\0', MAXSIZE);
            free(input);
            pthread_mutex_unlock(&senderLock);
            pthread_cond_signal(&senderCondition);
        }
    }
    if(ISDEBUG){printf("programInput: thread closed\n");}
    pthread_exit(NULL);

}

int main(int argc,char* argv[]) {
    if(argc != 4){
        printf("usage: local-port remote-machine-name remote-port\n");
        return 0;
    }
    printf("Port: %s RemoteMachine: %s RemotePort: %s\n", argv[1], argv[2], argv[3]);
    printf("Type '!' and press Enter to quit. This will close the session on both clients\n");

    //creation of two lists. send and receive
    senderList = ListCreate();
    receiverList = ListCreate();
    struct hostent *remoteServer;

    //assign arguments
    serverPort = argv[1];
    remoteMachine = argv[2];
    remotePort = argv[3];



    //socket initialization
    socketfd = socket(AF_INET,SOCK_DGRAM,0); //use IPV4, UDP, Default Protocol

    //receiver setup
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_addr.s_addr = INADDR_ANY; //tells server to listen to local IP
    serverAddress.sin_port = htons(atoi(serverPort)); //tells program to listen for incoming connections to port in argument
    memset(&serverAddress.sin_zero, '\0', 8);
    bind(socketfd, (struct sockaddr *) &serverAddress, sizeof(struct sockaddr_in));


    //client setup
    remoteServer = gethostbyname(remoteMachine);

    clientAddress.sin_family = AF_INET;
    memcpy(&clientAddress.sin_addr, remoteServer->h_addr_list[0], remoteServer->h_length);
    clientAddress.sin_port = htons(atoi(remotePort));


    //thread setup



    pthread_mutex_init(&receiverLock, NULL);
    pthread_mutex_init(&senderLock, NULL);

    pthread_cond_init(&receiverCondition, NULL);
    pthread_cond_init(&senderCondition, NULL);

    pthread_create(&receiver, NULL, *socketReceive, NULL);
    pthread_create(&output,NULL,*programOutput,NULL);

    pthread_create(&input,NULL,*programInput, NULL);
    pthread_create(&sender, NULL, *socketSend, NULL);



    pthread_join(sender, NULL);
    pthread_join(input, NULL);
    pthread_join(output, NULL);
    pthread_join(receiver, NULL);

    ListFree(receiverList, NULL);
    ListFree(senderList, NULL);

    close(socketfd);

    return 0;
}