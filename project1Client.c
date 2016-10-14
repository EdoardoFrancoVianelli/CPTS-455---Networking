//
//  main.c
//  455_Project1_Client
//
//  Created by Edoardo Franco Vianelli on 10/9/16.
//  Copyright © 2016 Edoardo Franco Vianelli. All rights reserved.
//

#include <stdio.h>
#include "project1.h"

int debug = 0;

ssize_t nullTerminatedCommandAction(int client_sock, command cmd){
    /* Send the string given as command.arg along with a null character as terminator
    (i.e. just like the string is conventionally represented in C). */
    
    unsigned long length = strlen(cmd.arg) + 1;
    char buf[100];
    memset(&buf, 0, sizeof(buf));
    
    buf[0] = nullTerminatedCmd; buf[1] = '\0';
    strcat(buf, cmd.arg);
    buf[length] = 0;
    
    return SendMsgToSocket(client_sock, buf, length);
}

ssize_t givenLengthCmdAction(int client_sock, command cmd){
    /* Convert command.arg to an int and send the 4 bytes without applying htonl() to the value.
     This is the incorrect way to go! Note that the number you get back from the server won’t
     be what was sent. */
    
    const int length_num_string = 10;
    char buf[256], length_string[length_num_string];
    
    uint16_t count = strlen(cmd.arg);
    snprintf(length_string, 10, "%d%d", givenLengthCmd, htonl(count));
    
    //put the length string in the final buf to write
    int i = 0;
    while (length_string[i] >= '0' && length_string[i] <= '9') //avoid putting a null terminator
    {
        buf[i] = length_string[i]; i++;
    }

    buf[i] = 0;
    
    int offset = i;
    
    while ((buf[i] = cmd.arg[i - offset]) && i < 256) {
        i++;
    }
    
    if (debug) {
        printf("About to send=[");
        for (i = 0; i < count+offset; i++){
            printf("%c", buf[i]);
        }
        printf("]\n");
    }
    
    return SendMsgToSocket(client_sock, buf, count+offset);
}


ssize_t sendIntCommand(int client_sock, command cmd, int n)
{
    char to_send[5];
    to_send[0] = cmd.cmd;
    
    char *ptr = (char*)&n;
    to_send[1] = *ptr++;
    to_send[2] = *ptr++;
    to_send[3] = *ptr++;
    to_send[4] = *ptr++;
    
    //printf("%x %x %x %x\n", to_send[1], to_send[2], to_send[3], to_send[4]);
    
    return SendMsgToSocket(client_sock, to_send, 5);
}

ssize_t badIntCmdAction(int client_sock, command cmd){
    /* Convert command.arg to an int and send the 4 bytes without applying htonl() to the value.
    This is the incorrect way to go! Note that the number you get back from the server won’t
    be what was sent.*/
    return sendIntCommand(client_sock, cmd, atoi(cmd.arg));
}

ssize_t goodIntCmdAction(int client_sock, command cmd){
    //Convert command.arg to an int and send the 4 bytes resulting from applying htonl() to it.
    return sendIntCommand(client_sock, cmd, htonl(atoi(cmd.arg)));
}

/*
 Convert command.arg to an int; send the int (after apply htonl) and then send
 *       that many bytes of alternating 1000-byte blocks of 0 bytes and 1 bytes.
 *       ByteAtATime - use 1-byte sends and receives
 *       KByteAtATime - use 1000-byte sends and receives (except for the last)
 */

ssize_t byteCommands(int socket_client, int bytes_time, command cmd){
    
    //alternate send and receive
    
    //send kByteAtATimeCmd
    char ptr_contents = cmd.cmd;
    SendMsgToSocket(socket_client, (char*)&ptr_contents, 1);
    
    int arg_int = atoi(cmd.arg);
    int converted = htonl(arg_int);
    
    char length_string[20];
    snprintf(length_string, 20, "%d", converted);
    
    SendMsgToSocket(socket_client, length_string, strlen(length_string));
    
    /***CONVERT CMD.ARG TO AN INTEGER
     ***APPLY HTONL TO IT AND SENT IT TO THE SERVER
     SendMsgToSocket(client_sock, (char*)&converted, 4);
     */
    
    int i;
    char cur = 0;
    
    if (debug){
        printf("About to send %d bytes\n", arg_int);
    }
    
    char  ones[1000]; memset(ones,  1, 1000);
    char zeros[1000]; memset(zeros, 0, 1000);
    
    for (i = 0; i < arg_int; i += bytes_time){
        if (i % 1000 == 0 && i > 0){
            if (cur == 0) {
                cur = 1;
            }
            else {
                cur = 0;
            }
        }
        if (cur == 0) {
            SendMsgToSocket(socket_client, zeros, bytes_time);
        }else{
            SendMsgToSocket(socket_client,  ones, bytes_time);
        }
    }
    
    return arg_int / bytes_time;
}

ssize_t byteAtTimeCmdAction(int client_sock, command cmd){
    return byteCommands(client_sock,    1, commands[4]);
}

ssize_t kByteAtATimeCmdAction(int client_sock, command cmd){
    return byteCommands(client_sock, 1000, commands[5]);
}

ssize_t noMoreCommandsAction(int client_sock, command cmd){
    printf("Closing socket %d\n", client_sock);
    close(client_sock);
    return 0;
}

ssize_t (*fptr[7])() = {
                    nullTerminatedCommandAction,
                    givenLengthCmdAction,
                    badIntCmdAction,
                    goodIntCmdAction,
                    byteAtTimeCmdAction,
                    kByteAtATimeCmdAction,
                    noMoreCommandsAction
                    };

unsigned short getShort(unsigned char* array, int offset)
{
    return (short)(((short)array[offset]) << 8) | array[offset + 1];
}

short getLength(int client_socket, char *remaining, char destination[]){
    //get one byte at a time until we encounter something other than a number
    int cumulative = 0;
    char buf[256];
    GetAtLeast(client_socket, buf, 256, 1, 0, 0);
    char current = buf[0];
    int iterated = 0;
    
    while (current >= '0' && current <= '9'){
        GetAtLeast(client_socket, buf, 256, 1, 0, 0);
        cumulative *= 10;
        cumulative += (int)(current - '0');
        current = buf[0];
        iterated = 1;
    }
    
    //if (iterated == 1){
    destination[0] = *remaining = current;
    destination[1] = 0;
    //}
    
    return ntohs(cumulative);
}

char *expected_responses[6] = { "Null Terminated: Sent as a null-terminated string",
    "Given Length: 50331648Sent as an unterminated string",
    "Bad Int: -175",
    "Good Int: 1684",
    "Byte At A Time: 50000",
    "KByte At A Time: 500" };

void DoCommands(int client_socket)
{
    char temp[256], remaining;
    char server_response[256];
    ssize_t server_res_len;
    ssize_t response_length;
    
    int i = 0;
    
    for (i = 0; i < 6; i++)
    {
        //RESET DATA BUFFERS
        memset(temp, 0, sizeof(temp)); memset(server_response, 0, sizeof(server_response)); remaining = 0;
        
        fptr[i](client_socket, commands[i]);
        
        server_res_len  = getLength(client_socket, &remaining, server_response);

        if (i == 4 || i == 5){
            server_res_len = strlen(commandNames[i + 1]) + number_length(atoi(commands[i].arg));
        }
        
        response_length = GetAtLeast(client_socket, temp, 256, server_res_len, 0,0);//need to get a buf of length server_res_len
        
        strcat(server_response, temp);
        
        if (debug == 1)
        {
            printf("%d) length=%zd ", i, response_length);
        }
        
        printf("%s\n", server_response);
    }
}

int main(int argc, const char * argv[]) {
    
    //Create a TCP stream socket
    
    if (argc != 3)
        printf("usage = <SERVER IP> <PORT>");
    
    const char *serverIP = argv[1];
    int port     = atoi(argv[2]);
    
    int client_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    
    sockaddr_in serverAddress;
    
    memset(&serverAddress, 0, sizeof(serverAddress));
    
    if (client_socket < 0)
        KillWithMessage("Could not create socket for client\n");
    
    serverAddress.sin_family         = AF_INET;
    serverAddress.sin_addr.s_addr    = inet_addr(serverIP);
    serverAddress.sin_port           = htons(port);
    
    int addr;
    
    if ((addr = serverAddress.sin_addr.s_addr) <= 0)
        KillWithMessage("Error with sin_addr.s_addr value");
    
    sockaddr *casted_addr = (sockaddr *)&serverAddress;
    
    if (casted_addr == 0)
        KillWithMessage("Failure to cast a proper server address\n");
    
    //printf("============Connecting============\n");
    int connection = connect(client_socket, casted_addr, sizeof(serverAddress));
    
    if (connection < 0)
        KillWithMessage("Failure to connection to server\n");
    
    //printf("=====Connection Successfull!======\n");
    
    DoCommands(client_socket);
    
    close(client_socket);
    
    return 0;
}
