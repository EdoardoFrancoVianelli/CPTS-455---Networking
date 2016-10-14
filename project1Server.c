//
//  main.c
//  455_Project1_Server
//
//  Created by Edoardo Franco Vianelli on 10/9/16.
//  Copyright © 2016 Edoardo Franco Vianelli. All rights reserved.
//

#include <stdio.h>
#include "project1.h"

#define MAX_CONNECTIONS 5
#define RESPONSE_LENGTH 256

//for the null terminated command: look for the null terminator

//for the givenLengthCmd, use the given length to know how much to read

FILE *session_file;
unsigned long total_bytes_read;
int debug = 0;

//idea taken from: http://stackoverflow.com/questions/7879953/converting-a-c-byte-array-to-a-long-long

typedef union number
{
    char charNum[4];
    unsigned long longNum;
} number;

//DR Hauser, I know you hate unions but this seemed like the most straigtforward way to do this

unsigned long FirstFourResponses(char response[],
                                 char length_string[],
                                 char incoming_data[],
                                 int index){
    
    unsigned short response_length = 2;//start with 2 as we must count the colon and the space
    response_length += strlen(incoming_data);
    response_length += strlen(commandNames[index]);
    
    snprintf(length_string, 16, "%u", htons(response_length));
    
    response_length += strlen(length_string);
    //now put the length in front of the string and concatenate the previously obtained response
    
    strcat(response, length_string);
    strcat(response, commandNames[index]);
    strcat(response, ": ");
    
    if (index == 3 || index == 4){
        //convert incoming data to a long
        number n;
        memset(n.charNum, *(int*)incoming_data, 4);
        unsigned long incoming_long = n.longNum;
        //put it back in the incoming data array
        snprintf(incoming_data, RESPONSE_LENGTH, "%d", (int)(incoming_long));
    }
    
    strcat(response, incoming_data);
    
    response[response_length] = 0;
    
    //printf("sending response = [%s]\n", response);
    
    return response_length;
}

unsigned long lengthOfNumber(unsigned long n){
    char temp[20];
    snprintf(temp, 10, "%lu", n);
    return strlen(temp);
}

unsigned long handleNumBytesAtATime(int socket, int num_bytes, command cmd){
    //get as many bytes as the length of htonl(command.arg)
    
    int num_arg = atoi(cmd.arg);
    unsigned long bytes_to_get = lengthOfNumber(htonl(num_arg));
    char length_buffer[20];
    GetAtLeast(socket, length_buffer, 20, bytes_to_get, &total_bytes_read, session_file);
    
    //now get 1 byte at a time
    
    if (debug){
        printf("About to receive %d bytes\n", num_arg);
    }
    
    int i, steps = 0;
    for (i = 0; i < num_arg; i += num_bytes){
        char buf[1000];
        GetAtLeast(socket, buf, num_bytes, num_bytes, &total_bytes_read, session_file);
        steps++;
    }
    
    return steps;
}

unsigned long handleByteAtATime(int socket){
    return handleNumBytesAtATime(socket,    1, commands[4]);
}

unsigned long handleKByteAtATime(int socket){
    return handleNumBytesAtATime(socket, 1000, commands[5]);
}

unsigned long HandleCommand(int index, int socket){
    unsigned long this_time_read = 0;
    char server_response[RESPONSE_LENGTH], length_string[16], buf[RESPONSE_LENGTH], full_client_response[RESPONSE_LENGTH] = {index + '0'};
    int identified = 1;
    ssize_t response_length = 0;
    
    memset(server_response, 0, 256); memset(length_string, 0, 16); memset(buf, 0, 256); memset(full_client_response, 0, 256);
    
    if (index <= 0) return 0;
    
    if (index <= 2){
        unsigned long cmd_len = strlen(commands[index - 1].arg);
        if (index == 2) cmd_len += lengthOfNumber(htonl(cmd_len)) - 1;
        this_time_read = GetAtLeast(socket, buf, sizeof(buf), cmd_len, &total_bytes_read, session_file); //get the rest of the response from server
        strcat(full_client_response, buf); //concatenate the index with the response
        response_length = FirstFourResponses(server_response, length_string, full_client_response, index);
    }
    else if (index <= 4){
        this_time_read = GetAtLeast(socket, buf, sizeof(buf), 4, &total_bytes_read, session_file);
        strcat(full_client_response, buf);
        response_length = FirstFourResponses(server_response, length_string, full_client_response, index);
    }
    else if (index < 7){
        if (index == 5){
            this_time_read = handleByteAtATime(socket);
        }
        else{
            this_time_read = handleKByteAtATime(socket);
        }
        full_client_response[0] = 0;
        strcat(full_client_response, commandNames[index]);
        strcat(full_client_response, ": ");
        snprintf(length_string, 16, "%lu", this_time_read);
        strcat(full_client_response, length_string);
        response_length = strlen(full_client_response);
        strcat(server_response, full_client_response);
    }
    
    if (identified == 1){
        SendMsgToSocket(socket, server_response, response_length);
    }
    return this_time_read;
}

int getCommandIndex(int clientSocket){
    //get the first character and return it as an integer
    char buf[1];
    GetAtLeast(clientSocket, buf, 1, 1, &total_bytes_read, session_file);
    return buf[0];
}

void HandleTCPClient(int clientSocket){
    int i, currentIndex;
    char buf[1] = { 7 };
    for (i = 0; i < 6; i++){
        currentIndex = getCommandIndex(clientSocket);
        if (i == 1) currentIndex -= '0';
        HandleCommand(currentIndex, clientSocket);
        SendMsgToSocket(clientSocket, buf, 1);
    }
}

int main(int argc, const char * argv[]) {
    
    if (argc != 2)
        KillWithMessage("usage = <SERVER PORT>\n");
    
    in_port_t echoServerPort = atoi(argv[1]);
    
    printf("Port = %hu\n", echoServerPort);
    
    sockaddr_in echoServAddr;
    
    int serv_sock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP); //create a socket for incoming connctions
    
    //BEGIN OF CODE TAKEN FROM http://beej.us/guide/bgnet/output/html/singlepage/bgnet.html#bind
    //TO AVOID THE ERROR IN BINDING
    
    int yes=1; //char yes='1'; // Solaris people use this
    // lose the pesky "Address already in use" error message
    if (setsockopt(serv_sock,SOL_SOCKET,SO_REUSEADDR,&yes,sizeof yes) == -1) {
        perror("setsockopt");
        exit(1);
    }
    
    //END OF CODE TO AVOID BINDING ERROR
    
    if (serv_sock < 0)
        KillWithMessage("Could not create a socket in first socket call\n");
    
    echoServAddr.sin_family = PF_INET; //Internet address family
    echoServAddr.sin_addr.s_addr = htonl(INADDR_ANY); //accept any incoming connection
    echoServAddr.sin_port = htons(echoServerPort); //local port
    
    int bind_res = bind(serv_sock, (sockaddr*)&echoServAddr, sizeof(echoServAddr)); //bind the socket
    
    if (bind_res < 0)
        KillWithMessage("Could not bind serv_sock\n");
    
    int listen_res = listen(serv_sock, MAX_CONNECTIONS);//make the socket listen for incoming connection
    
    
    if (listen_res < 0)
        KillWithMessage("Could not make socket listen\n");
    
    while (1)
    {
        total_bytes_read = 0;
        
        sockaddr_in clientAddress;
        socklen_t client_address_length = sizeof(clientAddress);
        sockaddr *casted_client = (sockaddr*)&clientAddress;
        if (casted_client == 0) //cast failed
            KillWithMessage("Casting client failed\n");
        //printf("=========Accepting new connections=========\n");
        int client_socket = accept(serv_sock, casted_client, &client_address_length);//wait for an incoming connection from a client
        //printf("============Connection accepted!===========\n");
        
        session_file = fopen("log.txt", "w+"); //open a log file for write
        
        if (client_socket < 0)
            KillWithMessage("Could not accept incoming connection\n");
        
        //we are now connected to a client
        HandleTCPClient(client_socket);
        
        fseek(session_file, 0L, SEEK_END);
        long sz = ftell(session_file);
        if (debug){
            printf("Size of the file is %lu\n", sz);
        }
        
        fclose(session_file);
        close(client_socket);
        
        printf("For this connection %lu bytes were read\n", total_bytes_read);
    }
    
    return 0;
}

/*
 
 return;
 
 //null terminated command
 
 currentIndex = getCommandIndex(clientSocket);
 total_bytes_read += HandleCommand(currentIndex, clientSocket);
 
 send(clientSocket, "", 1, 0);
 
 //given length
 
 currentIndex = getCommandIndex(clientSocket) - '0';
 total_bytes_read += HandleCommand(currentIndex, clientSocket);
 
 //bad int
 
 send(clientSocket, "", 1, 0);
 
 currentIndex = getCommandIndex(clientSocket);
 total_bytes_read += HandleCommand(currentIndex, clientSocket);
 
 //good int
 
 send(clientSocket, "", 1, 0);
 
 currentIndex = getCommandIndex(clientSocket);
 total_bytes_read += HandleCommand(currentIndex, clientSocket);
 
 return;
 
 //byte at a time command
 
 send(clientSocket, "", 1, 0);
 
 currentIndex = getCommandIndex(clientSocket);
 total_bytes_read += HandleCommand(currentIndex, clientSocket);
 
 */







