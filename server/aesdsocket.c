#include <stdlib.h>
#include <stdio.h>
#include <syslog.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>

#define BACKLOG 10
#define LOG_PATH "/var/tmp/aesdsocketdata"
#define BUFF 1024
#define PORT "9000"

int exitCode = 0;
int sckt = 0;

void signalHandler(int signalNumber)
{
    if (signalNumber==SIGINT || signalNumber==SIGTERM) {
        syslog(LOG_INFO, "Caught signal, exiting");
        exitCode = 1;
    }
}

void socketBindSetup()
{
    struct addrinfo hints;
    struct addrinfo* res;
    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_flags = AI_PASSIVE;
    hints.ai_socktype = SOCK_STREAM;
    if (getaddrinfo(NULL, PORT, &hints, &res)!=0) {
        syslog(LOG_ERR, "An error occurred setting up the socket.");
        exit(1);
    }
    sckt = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (sckt==-1) {
        syslog(LOG_ERR, "An error occurred setting up the socket: %s", strerror(errno));
        exit(1);
    }
    if (bind(sckt, res->ai_addr, res->ai_addrlen)==-1) {
        syslog(LOG_ERR, "An error occurred binding the socket: %s", strerror(errno));
        exit(1);
    }
    freeaddrinfo(res);
}

void socketserver(int socket)
{
    if (listen(socket, BACKLOG)<0) {
        syslog(LOG_ERR, "Error on listen: %s", strerror(errno));
        exit(1);
    }
    syslog(LOG_INFO, "Listening port 9000");
    int fileFD = open(LOG_PATH, O_CREAT | O_RDWR | O_TRUNC, 0666);
    long fileSize = 0;
    char* data = NULL;
    while (!exitCode) {
        struct sockaddr_storage clientAddress;
        socklen_t addressSize = sizeof clientAddress;

        int connectionFD = accept(socket, (struct sockaddr*) &clientAddress, &addressSize);
        if (connectionFD==-1) {
            if (errno==EINTR) break;
            else {
                syslog(LOG_ERR,
                        "An error occurred accepting a new connection to the socket: %s",
                        strerror(errno));
                exit(1);
            }
        }

        if (clientAddress.ss_family==AF_INET) {
            char address[INET6_ADDRSTRLEN];
            struct sockaddr_in* addressIn = (struct sockaddr_in*) &clientAddress;
            inet_ntop(AF_INET, &(addressIn->sin_addr), address, INET_ADDRSTRLEN);
            syslog(LOG_INFO, "Accepted connection from %s", address);
        }
        else if (clientAddress.ss_family==AF_INET6) {
            char address[INET6_ADDRSTRLEN];
            struct sockaddr_in6* addressIn6 = (struct sockaddr_in6*) &clientAddress;
            inet_ntop(AF_INET6, &(addressIn6->sin6_addr), address, INET6_ADDRSTRLEN);
            syslog(LOG_INFO, "Accepted connection from %s", address);
        }

        long recvCode;
        long index = 0;
        int chunks = 1;

        data = malloc(sizeof(char)*BUFF*chunks);
        if (!data) {
            syslog(LOG_ERR, "An error occurred on malloc: %s", strerror(errno));
            exit(1);
        }

        do {
            recvCode = recv(connectionFD, &data[index], BUFF, 0);
            if (recvCode==-1) {
                syslog(LOG_ERR,
                        "An error occurred reading from the socket: %s",
                        strerror(errno));
                exit(1);
            }
            index += recvCode;

            if (index!=0) {
                if (data[index-1]=='\n') {
                    if (lseek(fileFD, 0, SEEK_END)==-1) {
                        syslog(LOG_ERR,
                                "An error occurred on getting to the end of the file: %s",
                                strerror(errno));
                        exit(1);
                    }

                    long writtenBytes;
                    long writeLength = index;
                    char* writePtr = data;
                    while (writeLength!=0) {
                        writtenBytes = write(fileFD, writePtr, writeLength);
                        if (writtenBytes==-1) {
                            if (errno==EINTR) continue;
                            syslog(LOG_ERR,
                                    "An error occurred on writing to the file: %s",
                                    strerror(errno));
                            exit(1);
                        }
                        writeLength -= writtenBytes;
                        writePtr += writtenBytes;
                    }

                    fileSize += index;

                    if (lseek(fileFD, 0, SEEK_SET)==-1) {
                        syslog(LOG_ERR,
                                "An error occurred on getting to the beginning of the file: %s",
                                strerror(errno));
                        exit(1);
                    }

                    long sent = fileSize;
                    char readBuffer[BUFF];
                    while (sent) {
                        long sendBytes = 0;
                        long readBytes = read(fileFD, readBuffer, BUFF);
                        if (readBytes!=0) sendBytes = readBytes;
                        if (readBytes==-1) {
                            if (errno==EINTR) continue;
                            syslog(LOG_ERR,
                                    "An error occurred on reading from the file: %s",
                                    strerror(errno));
                            exit(1);
                        }

                        sent -= readBytes;

                        long sentBytes = -1;
                        long sendOff = 0;
                        syslog(LOG_INFO, "Send bytes: %ld", sendBytes);
                        while (sentBytes!=0) {
                            sentBytes = send(connectionFD, &readBuffer[sendOff], sendBytes, 0);
                            if (sentBytes==-1) {
                                if (errno==EINTR)continue;
                                syslog(LOG_ERR,
                                        "An error occurred on reading from the file: %s",
                                        strerror(errno));
                                exit(1);
                            }
                            sendBytes -= sentBytes;
                            sendOff += sentBytes;
                        }
                    }
                    index = 0;
                }
                else if (index==(BUFF*chunks)) {
                    chunks++;
                    data = realloc(data, sizeof(char)*BUFF*chunks);
                    if (!data) {
                        syslog(LOG_ERR, "An error occurred on realloc: %s", strerror(errno));
                        exit(1);
                    }
                }
            }
        }
        while (recvCode!=0 && !exitCode);

        free(data);
        if (clientAddress.ss_family==AF_INET) {
            char address[INET6_ADDRSTRLEN];
            struct sockaddr_in
                    * addressIn = (struct sockaddr_in*) &clientAddress;
            inet_ntop(AF_INET, &(addressIn->sin_addr), address, INET_ADDRSTRLEN);
            syslog(LOG_INFO, "Closed connection from %s", address);
        }
        else if (clientAddress.ss_family==AF_INET6) {
            char address[INET6_ADDRSTRLEN];
            struct sockaddr_in6* addressIn6 = (struct sockaddr_in6*) &clientAddress;
            inet_ntop(AF_INET6, &(addressIn6->sin6_addr), address, INET6_ADDRSTRLEN);
            syslog(LOG_INFO, "Closed connection from %s", address);
        }
    }
    close(socket);
    close(fileFD);
    remove(LOG_PATH);
}

void signalHandlerSetup()
{
    struct sigaction exitAction;
    exitAction.sa_handler = signalHandler;
    exitAction.sa_flags = 0;
    sigset_t empty;
    if (sigemptyset(&empty)==-1) {
        syslog(LOG_ERR, "Could not set up empty signal set: %s.", strerror(errno));
        exit(1);
    }
    exitAction.sa_mask = empty;
    if (sigaction(SIGINT, &exitAction, NULL)!=0) {
        syslog(LOG_ERR, "Could not set up handle for SIGINT: %s.", strerror(errno));
        exit(1);
    }
    if (sigaction(SIGTERM, &exitAction, NULL)!=0) {
        syslog(LOG_ERR, "Could not set up handle for SIGTERM: %s.", strerror(errno));
        exit(1);
    }
}

int main(int c, char** argv)
{
    openlog(NULL, 0, LOG_USER);
    signalHandlerSetup();
    socketBindSetup();
    if (c==1) {
        syslog(LOG_INFO, "Server running in no-daemon mode.");
        socketserver(sckt);
    }
    else if (c==1+1) {
        if (strcmp(argv[1], "-d")!=0) {
            syslog(LOG_ERR, "Invalid number of arguments");
            exit(1);
        }

        syslog(LOG_INFO, "Server running in daemon mode.");

        int forkCode = fork();
        if (forkCode==-1) {
            syslog(LOG_ERR, "Unable to fork daemon creation: %s.", strerror(errno));
            exit(1);
        }
        else if (forkCode==0) {
            if (setsid()==-1) {
                syslog(LOG_ERR,
                        "Unable to create a new session and process group: %s",
                        strerror(errno));
                exit(1);
            }
            if (chdir("/")==-1) {
                syslog(LOG_ERR, "Unable to change working directory: %s", strerror(errno));
                exit(1);
            }
            socketserver(sckt);
            exit(0);
        }
        exit(0);
    }
    else {
        syslog(LOG_ERR, "Invalid number of arguments");
        exit(1);
    }
    return 0;
}