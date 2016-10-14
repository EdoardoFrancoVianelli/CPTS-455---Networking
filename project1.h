#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <errno.h>
#include <arpa/inet.h>
#include <unistd.h>

#define NUM_COMMANDS 7

typedef struct {
	unsigned cmd;
	char *arg;
} command;

#define nullTerminatedCmd  (1)
#define givenLengthCmd  (2)
#define badIntCmd  (3)
#define goodIntCmd  (4)
#define byteAtATimeCmd  (5)
#define kByteAtATimeCmd  (6)
#define noMoreCommands (0)
/* This is the list of commands to be run by the client to demonstrate your program */

static command commands[] = {
	{nullTerminatedCmd, "Sent as a null-terminated string"},
	{givenLengthCmd, "Sent as an unterminated string"},
	{badIntCmd, "20160919"},
	{goodIntCmd, "20160919"},
	{byteAtATimeCmd, "500000"},
	{kByteAtATimeCmd, "500000"},
	{noMoreCommands, ""}
};

//00000001
//00110011
//10100001
//10010111

/*
* These command names are to be prefixed to responses by the server; the array is
* indexed by the #define’d constants above
*/

static char *commandNames[] = {
	"No More Commands",
	"Null Terminated",
	"Given Length",
	"Bad Int",
	"Good Int",
	"Byte At A Time",
	"KByte At A Time"
};

typedef struct byte_group{
    unsigned char b1;
    unsigned char b2;
    unsigned char b3;
    unsigned char b4;
}byte_group;

typedef struct sockaddr_in sockaddr_in;
typedef struct sockaddr sockaddr;

int StartsWithNumbersAndContinuesWithAllowedCharacters(char input[], char min, char max);

int KillWithMessage(char *message)
{
	printf("%s, error = %s\n", message, strerror(errno));
	exit(-1);
}

ssize_t GetAtLeast(int sock,
                   char *buf,
                   int bufSize,
                   unsigned long atLeast,
                   unsigned long *or_atleast,
                   FILE *file_ptr)
{
    ssize_t bytesRead = 0;
    while (bytesRead < atLeast)
    {
        ssize_t thisTime = recv(sock, buf, atLeast, 0);
        
        if (file_ptr){ fputs(buf, file_ptr); }
        
        bytesRead += thisTime;
        if (thisTime <= 0){
            close(sock);
            return bytesRead;
        }
    }
    
    if (or_atleast) { *or_atleast += bytesRead; }
    
    return bytesRead;
}

ssize_t SendMsgToSocket(int sock, char *buf, unsigned long length){
    ssize_t size_sent = send(sock, buf, length, 0);
    
    if (size_sent < length){
        close(sock);
        KillWithMessage("Partial send on socket\n");
    }
    
    return size_sent;
}

int IsLetter(char c)
{
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

int IsNumber(char c){
    return (c >= '0' && c <= '9');
}

int lower(int a) // http://stackoverflow.com/questions/15708793/c-convert-an-uppercase-letter-to-lowercase
{
    if ((a >= 65) && (a <= 90))
        a = a + 32;
    return a;
}

int StartsWithNumbersAndContinuesWithAllowedCharacters(char input[], char min, char max){
    int number = 0;
    int firstIsNumber = IsNumber(input[0]);
    if (firstIsNumber == 0) return 0;
    int i = 1;
    
    while (1){
        int thisTime;
        //char lower_case = lower(input[i]);
        if ((thisTime = IsNumber(input[i]))){
            number *=       10;
            number += thisTime;
        }else{
            return number;
        }
        i++;
    }
    
    return 0;
}

void IntToBytes(int n, char buf[], int offset)
{
    //make a pointer
    
    char *ptr = (char*)&n;
    int i;
    
    for (i = 0; i < 4; i++){
        buf[i + offset] = htons(*ptr++);
    }
}

int number_length(ssize_t n){
    int count = 0;
    while (n > 0) {
        count++;
        n /= 10;
    }
    return count;
}

/* The maximum argument string length is 128 bytes */
/*
* Client behavior:
*   For noMoreCommands:
*       don’t send anything; exit the command-reading loop and
*       close the socket.
*   For nullTerminatedCmd:
*       Send the string given as command.arg along with a null character as terminator
*       (i.e. just like the string is conventionally represented in C).
*   For givenLengthCmd:
*       Send the string’s length as a 16 bit number in network byte order followed by the
*       characters of the string; do not include a null character.
*   For badIntCmd:
3
*       Convert command.arg to an int and send the 4 bytes without applying htonl() to the value.
*       This is the incorrect way to go! Note that the number you get back from the server won’t
*       be what was sent.
*   For goodIntCmd:
*       Convert command.arg to an int and send the 4 bytes resulting from applying htonl() to it.
*   For byteAtATimeCmd and kByteAtATimeCmd:
*       Convert command.arg to an int; send the int (after apply htonl) and then send
*       that many bytes of alternating 1000-byte blocks of 0 bytes and 1 bytes.
*       ByteAtATime - use 1-byte sends and receives
*       KByteAtATime - use 1000-byte sends and receives (except for the last)
*   After sending the message associated with a command, recv the response
*   that the server produces for that command and print it on stdout followed by a \n
*/
/*
* Server behavior:
*   The server never receives noMoreCommands.
*
*   For nullTerminatedCmd, givenLengthCmd, badIntCmd, goodIntCmd:
*   reply with a 16 bit string length followed by a string containing
*   the name of the command (i.e. commandNames[cmdByte]),
*   a colon, a space, and the received value. Example:
*      16bit: htons(11)
*      11 bytes: GoodInt: 37
*   Note that the terminator for the string sent by nullTerminatedCmd
*   is *not* considered part of the string and should not be echoed.
*
*   The server behavior for BadInt and GoodInt is identical -- it applies
*   ntohl to the received bytes.
*
*   For byteAtATimeCmd and kByteAtATimeCmd reply with the name of the command and the
*   total number of recv() operations that the server performed in carrying it out,
*   formatted as an ASCII string and counting the first recv of the data bytes as 1.
*   Use the same format as before (16 bit network byte order integer, followed by the
*   bytes of the string with no terminator character).
*
*   In addition to its other behavior, the server is to write all the bytes that
*   it receives into a file -- it doesn’t matter where as long as you can find it later.
*   Make sure to write all and only the bytes the server receives into the file.
*
*   The server should close the client connection when it recv’s 0 bytes, which indicates that the
*   client has closed its end of the connection. Print on the console (printf) the total number
*   of bytes received on the current connection. Then continue by calling accept() for a new
*   connection.
*/
