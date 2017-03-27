#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <sys/sendfile.h>
#include <sys/stat.h>
#include <fcntl.h>

#define MAXPENDING 5

struct field
{
    char* name;
    char* value;
};
void DieWithError(char*);
void recvMessagePrint(char*);
void sendMessagePrint(char*);
void Error(char*);
void startNodeServer(int);
void servUtil(int,char *,int);
int putfile(int,const char*);
struct field * packet_parser(char *);
char* getfieldvalue(struct field*,char *);
int sendack(int);
int checkack(int);

int main(int argc,char **argv)
{
    if(argc!=3){
        DieWithError("<executable code> <Server IP Address> <Server Port number>");
    }

    int nodeSock;
    int servPort = atoi(argv[2]);
    char* servIP = argv[1];

    if((nodeSock = socket(AF_INET, SOCK_STREAM, 0)) < 0){
    	DieWithError("socket() error");
    } 

    struct sockaddr_in servAddr;
    memset(&servAddr, '\0', sizeof(servAddr)); 

    servAddr.sin_family = AF_INET;
    servAddr.sin_port = htons(servPort);
    servAddr.sin_addr.s_addr = inet_addr(servIP);

    if( connect(nodeSock, (struct sockaddr *)&servAddr, sizeof(servAddr)) < 0){
       DieWithError("connect() failed");
    }
    else{
        printf("Connected to server %s:%d\n",servIP,servPort);
    }

    int recvMsgSize,sendMsgSize;
    char* request = "request:node";
    
    if((sendMsgSize=write(nodeSock,request,strlen(request))) < 0){
        DieWithError("write() request error");
    }
    else{
        printf("Registering with server %s:%d\n",servIP,servPort);
    }
    
    sendMessagePrint(request);

    char buffer[1024];
    memset(buffer,'\0',sizeof(buffer));

    if((recvMsgSize = read(nodeSock,buffer,sizeof(buffer) - 1)) < 0){
    	DieWithError("read() error");
    }

    recvMessagePrint(buffer);

    char* responsefield = strtok(buffer,"\n");
    char* statusfield = strtok(NULL,"\n");
    char* portfield = strtok(NULL,"\n");
    if(strcmp(responsefield,"response:server") != 0 || strcmp(statusfield,"status:connected")!=0){
        DieWithError("Unexpected response from server");
    }
    else{
        portfield = strtok(portfield,":");
        portfield = strtok(NULL,":");
        if(shutdown(nodeSock,0)< 0){
            DieWithError("shutdown() error");
        }
        else{
            printf("Registered with server %s:%d\n",servIP,servPort);
            printf("Gracefully closing connection with server %s:%d\n",servIP,servPort );
        }
        int nodeServerPort = atoi(portfield);
        startNodeServer(nodeServerPort);
    }

    return 0;
}

void startNodeServer(int port){

    int servPort = port;
    int servSock;

    if((servSock=socket(AF_INET,SOCK_STREAM, 0)) < 0){
        DieWithError("socket() failed");
    }

    struct sockaddr_in servAddr;
    memset((void *)&servAddr,'\0',sizeof(servAddr));

    servAddr.sin_family = AF_INET;
    servAddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servAddr.sin_port = htons(servPort);

    if(bind(servSock,(struct sockaddr*) &servAddr, sizeof(servAddr)) < 0){
        DieWithError("bind() failed");
    }

    if(listen(servSock,MAXPENDING) < 0){
        DieWithError("listen() failed");
    }
    else{
        printf("Node server listening on port %d\n",servPort);
    }

    while(1){

        int clntLen,clientSock;
        struct sockaddr_in clntAddr;

        clntLen = sizeof(clntAddr);

        int client_Port = ntohs(clntAddr.sin_port);
        char client_IP[INET_ADDRSTRLEN];
        if((clientSock=accept(servSock,(struct sockaddr *)&clntAddr,&clntLen)) < 0){
            DieWithError("accept() failed");
        }
        else{

            memset(client_IP,'\0',sizeof(client_IP));
            if(inet_ntop(AF_INET, &(clntAddr.sin_addr),client_IP,sizeof(client_IP)) == 0){
                DieWithError("inet_ntop() error");
            }
            printf("Client %s:%d accepted\n", client_IP,client_Port);
        }

        int pid = fork();

        if(pid < 0){
            DieWithError("fork() error");
        }

        if(pid == 0){
            close(servSock);
            servUtil(clientSock,client_IP,client_Port);
            exit(EXIT_SUCCESS);
        }
        else{
            close(clientSock);
        }
    }
}

void servUtil(int clientSock,char *client_IP,int client_Port){
    int recvMsgSize, sendMsgSize;
    char buffer[1024];
    memset(buffer,'\0',sizeof(buffer));

    if(recvMsgSize = read(clientSock,buffer,sizeof(buffer) - 1) < 0){
        DieWithError("read() error");
    }
    recvMessagePrint(buffer);
    if(strcmp(buffer,"request:client") != 0){
        DieWithError("Unexpected request");
    }
    sendack(clientSock);

    memset(buffer,'\0',sizeof(buffer));
    if(recvMsgSize = read(clientSock,buffer,sizeof(buffer) - 1) < 0){
        DieWithError("read() error");
    }
    recvMessagePrint(buffer);
    sendack(clientSock);

    char* filefield;
    filefield = getfieldvalue(packet_parser(buffer),"filename");
    printf("Client %s:%d fetch request for file '%s'\n",client_IP,client_Port, filefield);
    if(putfile(clientSock,filefield) == 0){
        fprintf(stdout,"File '%s' sent\n", filefield);
    }
    else{
        fprintf(stdout,"File '%s' not found\n", filefield);
    }
}

int sendack(int clientSock){
    char buffer[1024];
    int recvMsgSize,sendMsgSize;

    strcpy(buffer,"response:node\nack:1");
    if((sendMsgSize = write(clientSock,buffer,strlen(buffer))) < 0){
        DieWithError("request write() ack error");
    }
    sendMessagePrint(buffer);
}

int checkack(int clientSock){
    char buffer[1024];
    int recvMsgSize;
    memset(buffer,'\0',sizeof(buffer));
    if((recvMsgSize = read(clientSock,buffer,sizeof(buffer) - 1)) < 0){
        return -1;
    }
    recvMessagePrint(buffer);
    if(strcmp(getfieldvalue(packet_parser(buffer),"ack"),"1") != 0){
        return -1;
    }
    return 0;
}

int putfile(int clientSock,const char * filename){
    int recvMsgSize, sendMsgSize;
    // FILE *fp = fopen(filename,"r");
    // if(fp==NULL){
    //     return 0;
    // }
    // fclose(fp);
    char response[128];
    int fd;
    struct stat file_stat;
    if((fd = open(filename,O_RDONLY)) < 0){
        // DieWithError("Error opening file");
        strcpy(response,"response:node\nfile:#####");
        if((sendMsgSize = write(clientSock,response,strlen(response))) < 0){
            DieWithError("Could not send node server response\n");
        }
        sendMessagePrint(response);
        if(checkack(clientSock) < 0){
            DieWithError("No ack recieved");
        }
        return -1;
    }

    strcpy(response,"response:node\nfile:yes");
    if((sendMsgSize = write(clientSock,response,strlen(response))) < 0){
        DieWithError("write () error");
    }
    sendMessagePrint(response);
    if(checkack(clientSock) < 0){
        DieWithError("No ack recieved");
    }


    if(fstat(fd,&file_stat)<0){
        DieWithError("fstsat() error");
    }
    char buffer[1024];
    sprintf(buffer,"response:node\nfile:%s\nfilesize:%d",filename,(int)file_stat.st_size);

    if((sendMsgSize = write(clientSock,buffer,strlen(buffer))) < 0){
        DieWithError("write() error while sending filesize response");
    }

    sendMessagePrint(buffer);

    if(checkack(clientSock) < 0){
        DieWithError("No ack recieved");
    }
    
    off_t offset = 0,remain_data = file_stat.st_size;
    float z = remain_data/1024.0;
    printf("File info - Filename : %s, Size : %.3f KB\n",filename,z);

    while((sendMsgSize = sendfile(clientSock,fd,&offset,BUFSIZ)) > 0 && remain_data){
        // printf("1. Server sent %d bytes from file's data and remaining data = %d\n", sendMsgSize, (int)remain_data);
        remain_data -= sendMsgSize;
        // printf("2. Server sent %d bytes from file's data and remaining data = %d\n", sendMsgSize, (int)remain_data);
        
        if(checkack(clientSock) < 0){
            DieWithError("No ack recieved");
        }
    }
    return 0;
}

struct field * packet_parser(char *buffer){
    struct field* packet_fields = (struct field*)malloc(sizeof(struct field)*32);
    int fields_num = 1;
    char* temp = strtok(buffer,"\n");
    while(temp){
        packet_fields[fields_num++].name = temp;
        temp = strtok(NULL,"\n");
    }

    char* fields_num_string = (char*)malloc(sizeof(char)*32);
    sprintf(fields_num_string,"%d",fields_num);

    packet_fields[0].name = NULL;
    packet_fields[0].value = fields_num_string;

    int i;
    for(i = 1;i<fields_num;i++){
        temp = strtok(packet_fields[i].name,":");
        packet_fields[i].name = temp;
        temp = strtok(NULL,":");
        packet_fields[i].value = temp;
    }

    return packet_fields;
}

char * getfieldvalue(struct field* packet,char *field){
    int n = atoi(packet[0].value);
    while(n>0 && strcmp(packet[--n].name,field)!=0);
    if(n<=0){
        return NULL;
    }
    return packet[n].value;
}

void DieWithError(char *s){
    printf("%s\n",s);
    exit(EXIT_SUCCESS);
}

void Error(char *s){
    printf("%s\n", s);
}

void recvMessagePrint(char *s){
    // printf("Message recieved = [%s]\n", s);
}

void sendMessagePrint(char *s){
    // printf("Message sent = [%s]\n", s);
}
