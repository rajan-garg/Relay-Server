#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <fcntl.h>

#define MAXPENDING 5
#define TEMPFILENAME "#1#2#3#4#5#6"
#define DOWNLOADFILECOPY "copy.txt"

struct field
{
    char* name;
    char* value;
};

struct peernodes
{
    char address[64];
    int port;
};

void DieWithError(char*);
void socketUtil(int,struct sockaddr_in *);
void recvMessagePrint(char*);
void sendMessagePrint(char*);
void StoreAddressPort(char*,int);
void Error(char*);
struct field * packet_parser(char *);
char* getfieldvalue(struct field*,char *);
struct peernodes* getnodeslist(int);
int getfile(int,char *);
int getfile2(int,char *);
int checkack(int);
void clientUtil(struct peernodes*);

int main(int argc,char** argv)
{
    if(argc!=3){
        DieWithError("<executable code> <Server IP Address> <Server Port number>");
    }

    int clientSock;
    int servPort = atoi(argv[2]);
    char* servIP = argv[1];

    if((clientSock = socket(AF_INET, SOCK_STREAM, 0)) < 0){
    	DieWithError("socket() error");
    } 

    struct sockaddr_in servAddr;
    memset(&servAddr, '\0', sizeof(servAddr)); 

    servAddr.sin_family = AF_INET;
    servAddr.sin_port = htons(servPort);
    servAddr.sin_addr.s_addr = inet_addr(servIP);

    if( connect(clientSock, (struct sockaddr *)&servAddr, sizeof(servAddr)) < 0){
       DieWithError("connect() failed");
    }
    else{
        printf("Connected to server with address %s\n",servIP);
    }

    char buffer[1024];
    int recvMsgSize,sendMsgSize;
    
    strcpy(buffer,"request:client");

    if((sendMsgSize = write(clientSock,buffer,strlen(buffer))) < 0){
    	DieWithError("write() error");
    }
    sendMessagePrint(buffer);

    memset(buffer,'\0',sizeof(buffer));
    if((recvMsgSize = read(clientSock,buffer,sizeof(buffer) - 1)) < 0){
    	DieWithError("read() error");
    }
    recvMessagePrint(buffer);

    sendack(clientSock);

    struct field *packet = packet_parser(buffer);
    printf("Peer nodes list fetched from server\n");

    if(strcmp(getfieldvalue(packet,"status"),"connected")!=0){
        DieWithError("Unexpected response from server");
    }
    
    struct peernodes * nodeslist = getnodeslist(clientSock);
    
    if(shutdown(clientSock,0)< 0){
        DieWithError("shutdown() error closing connection with server.");
    }
    else{
        printf("Connection with server closed gracefully\n");
    }
    
    char c[10] = "y";
    while(1){
        
        if(strcmp("Y",c)==0 || strcmp("y",c)==0){
            clientUtil(nodeslist);
        }
        else if(strcmp("N",c)==0 || strcmp("n",c)==0){
            printf("Client exiting....\n");    
            break;
        }
        else{
            printf("Unknown option\n");
        }
        printf("Press (N\\n) to quit or (Y\\y) to continue\n");
        scanf("%s",c);
    }

    return 0;
}

void clientUtil(struct peernodes * nodeslist){

    int i,num_nodes=nodeslist[0].port;

    char filename[64];
    printf("File to download : ");
    scanf("%s",filename);

    int flag = 0;
    for(i = 1;i<num_nodes-1;i++){
        // printf("%s %d\n",nodeslist[i].address,nodeslist[i].port);
        if(connectnode(nodeslist[i].address,nodeslist[i].port,filename) == 0){
            flag = 1;
            break;
        }
    }

    if(flag == 1){
        printf("File '%s' found on peer %s:%d\n",filename,nodeslist[i].address,nodeslist[i].port);
    }
    else{
        printf("File '%s' not found on any peer node\n",filename);
    }

}

int connectnode(char* servIP,int servPort,char* filename){
    
    int clientSock;
    if((clientSock = socket(AF_INET, SOCK_STREAM, 0)) < 0){
        return -1;
    } 

    struct sockaddr_in servAddr;
    memset(&servAddr, '\0', sizeof(servAddr)); 

    servAddr.sin_family = AF_INET;
    servAddr.sin_port = htons(servPort);
    servAddr.sin_addr.s_addr = inet_addr(servIP);

    if( connect(clientSock, (struct sockaddr *)&servAddr, sizeof(servAddr)) < 0){
        close(clientSock);
        return -1;
    }
    else{
        printf("Connected to peer %s:%d\n",servIP,servPort);
    }

    char buffer[1024];
    int recvMsgSize,sendMsgSize;
    
    strcpy(buffer,"request:client");

    if((sendMsgSize = write(clientSock,buffer,strlen(buffer))) < 0){
        close(clientSock);
        return -1;
    }

    sendMessagePrint(buffer);
    if(checkack(clientSock) < 0){
        close(clientSock);
        return -1;
    }

    sprintf(buffer,"request:client\nfilename:%s",filename);
    if((sendMsgSize = write(clientSock,buffer,strlen(buffer))) < 0){
        close(clientSock);
        return -1;
    }
    
    sendMessagePrint(buffer);
    if(checkack(clientSock) < 0){
        close(clientSock);
        return -1;
    }

    memset(buffer,'\0',sizeof(buffer));
    if((recvMsgSize = read(clientSock,buffer,sizeof(buffer) - 1)) < 0){
        close(clientSock);
        return -1;
    }
    recvMessagePrint(buffer);
    sendack(clientSock);

    if(strcmp(getfieldvalue(packet_parser(buffer),"file"),"yes") ==0){
        if(getfile2(clientSock,TEMPFILENAME) < 0){
            close(clientSock);
            return -1;
        }
        FILE* fp1 = fopen(TEMPFILENAME,"r");
        FILE* fp2 = fopen(DOWNLOADFILECOPY,"w");
        // printf("File '%s' found on peer %s:%d\n",filename,servIP,servPort);
        char c;
        c = getc(fp1);
        while(c!=EOF){
            ungetc(c,fp1);
            memset(buffer,'\0',sizeof(buffer));
            fgets(buffer,sizeof(buffer)-1,fp1);
            printf("%s", buffer);
            fprintf(fp2,"%s\n",buffer);
            c = getc(fp1);
        }
        printf("\n");
        fclose(fp1);
        fclose(fp2);
        printf("Fetched file in %s\n", DOWNLOADFILECOPY);
        remove(TEMPFILENAME);
        printf("Closing connection with peer %s:%d gracefully\n", servIP,servPort);
        shutdown(clientSock,0);
        return 0;
    }
    else{
        // printf("File '%s' not found on peer %s:%d\n",filename,servIP,servPort);
        printf("Closing connection with peer %s:%d gracefully\n", servIP,servPort);
        shutdown(clientSock,0);
        return -1;
    }
    
}

struct peernodes* getnodeslist(int clientSock){
    char address[64];
    char port[16];
    int recvMsgSize,sendMsgSize;
    struct peernodes* nodeslist = (struct peernodes*)malloc(sizeof(struct peernodes)*128);

    if(getfile(clientSock,TEMPFILENAME) < 0){
        DieWithError("getfile() error while fetching peer nodes list");
    }

    
    FILE* fp = fopen(TEMPFILENAME,"r");

    int count = 1;
    char c;
    c = getc(fp);
    while(c!=EOF){
        ungetc(c,fp);

        fscanf(fp,"%s",address);
        fscanf(fp,"%s",port);
        strcpy(nodeslist[count].address,address);
        nodeslist[count++].port = atoi(port);
        c = getc(fp);
    }
    
    remove(TEMPFILENAME);

    memset(nodeslist[0].address,'\0',sizeof(nodeslist[0].address));
    nodeslist[0].port = count;
    return nodeslist;
}

int getfile(int clientSock,char * filename){
    char buffer[1024];
    int recvMsgSize,sendMsgSize;

    if((recvMsgSize = read(clientSock,buffer,sizeof(buffer) - 1)) < 0){
        DieWithError("read() error nodes list size");
    }
    recvMessagePrint(buffer);

    sendack(clientSock);
    FILE* fp = fopen(filename,"w");
    if(fp == NULL){
        return -1;
    }
    struct field* packet = packet_parser(buffer);
    int buffer_size = atoi(getfieldvalue(packet,"filesize"));
    int remain_data = buffer_size;

    memset(buffer,'\0',sizeof(buffer));
    while((recvMsgSize = read(clientSock,buffer,sizeof(buffer) - 1)) > 0 && remain_data>0){
        fprintf(fp, "%s", buffer);
        remain_data -= recvMsgSize;
        // printf("Receive %d bytes and remaining :- %d bytes\n", recvMsgSize, remain_data);
        
        memset(buffer,'\0',sizeof(buffer));
        sendack(clientSock);
    }
    fclose(fp);
    return 0;
}

int getfile2(int clientSock,char * filename){
    char buffer[1024];
    int recvMsgSize,sendMsgSize;

    if((recvMsgSize = read(clientSock,buffer,sizeof(buffer) - 1)) < 0){
        return -1;
    }
    recvMessagePrint(buffer);

    sendack(clientSock);
    FILE* fp = fopen(filename,"w");
    if(fp == NULL){
        return -1;
    }
    struct field* packet = packet_parser(buffer);
    int buffer_size = atoi(getfieldvalue(packet,"filesize"));
    int remain_data = buffer_size;

    memset(buffer,'\0',sizeof(buffer));
    while((recvMsgSize = read(clientSock,buffer,sizeof(buffer) - 1)) > 0 && remain_data>0){
        fprintf(fp, "%s", buffer);
        remain_data -= recvMsgSize;
        // printf("Receive %d bytes and remaining :- %d bytes\n", recvMsgSize, remain_data);
        
        memset(buffer,'\0',sizeof(buffer));
        sendack(clientSock);
    }
    fclose(fp);
    return 0;
}

int sendack(int clientSock){
    char buffer[1024];
    int recvMsgSize,sendMsgSize;

    strcpy(buffer,"request:client\nack:1");
    if((sendMsgSize = write(clientSock,buffer,strlen(buffer))) < 0){
        DieWithError("write() ack send error");
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

char * getfieldvalue(struct field* packet,char* field){
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

void StoreAddressPort(char *IP, int port){
    FILE* fp = fopen("nodeslist.txt","a+");
    fprintf(fp, "%s %d\n",IP,port);
    fclose(fp);
}

void recvMessagePrint(char *s){
    // printf("Message recieved = [%s]\n", s);
}

void sendMessagePrint(char *s){
    // printf("Message sent = [%s]\n", s);
}
