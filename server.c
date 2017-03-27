#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <fcntl.h>

#define MAXPENDING 5
#define NODESLIST "nodeslist.txt"

struct field
{
    char* name;
    char* value;
};

void DieWithError(char*);
void socketUtil(FILE*,int,struct sockaddr_in *);
void noderequest(FILE*,int,struct sockaddr_in *);
void clientrequest(int,struct sockaddr_in *);
void recvMessagePrint(char*);
void sendMessagePrint(char*);
void StoreAddressPort(FILE*,char*,int);
void Error(char*);
struct field * packet_parser(char *);
char* getfieldvalue(struct field*,char *);
int putfile(int,char*);


int main(int argc,char **argv){
	//takes a port as input
	if(argc!=2){
		DieWithError("<executable code> <port number>");
	}
	//this is port
	int servPort = atoi(argv[1]);
	int servSock;
	//return the fd of the socket, first call to socket() function 
	if((servSock=socket(AF_INET,SOCK_STREAM, 0)) < 0){
		DieWithError("socket() failed");
	}
	// Initialize socket structure
	struct sockaddr_in servAddr;
	memset((void *)&servAddr,'\0',sizeof(servAddr));

	servAddr.sin_family = AF_INET;
	servAddr.sin_addr.s_addr = htonl(INADDR_ANY);
	servAddr.sin_port = htons(servPort);
	/* Now bind the host address using bind() call.*/
	if(bind(servSock,(struct sockaddr*) &servAddr, sizeof(servAddr)) < 0){
		DieWithError("bind() failed");
	}
	 /* Now start listening for the clients, here process will
      * go in sleep mode and will wait for the incoming connection
   */
	if(listen(servSock,MAXPENDING) < 0){
		DieWithError("listen() failed");
	}
	else{
		printf("Relay server listening on port %d\n", servPort);
	}


	FILE* fp = fopen(NODESLIST,"w");

	while(1){

		int clntLen,clientSock;
		struct sockaddr_in clntAddr;

		clntLen = sizeof(clntAddr);
		 /* Accept actual connection from the client */
		if((clientSock=accept(servSock,(struct sockaddr *)&clntAddr,&clntLen)) < 0){
			DieWithError("accept() failed");
		}

		int pid = fork();

		if(pid < 0){
			DieWithError("fork() error");
		}

		if(pid == 0){
			close(servSock);
			socketUtil(fp,clientSock,&clntAddr);
			exit(EXIT_SUCCESS);
		}
		else{
			close(clientSock);
		}
	}
	
	return 0;
}

void socketUtil(FILE* fp,int clientSock, struct sockaddr_in * clntAddr){
	int recvMsgSize, sendMsgSize;
	char buffer[1000];
	memset(buffer,'\0',sizeof(buffer));

	if(recvMsgSize = read(clientSock,buffer,sizeof(buffer) - 1) < 0){
		DieWithError("read() error");
	}

	recvMessagePrint(buffer);
	
	if(strcmp(buffer,"request:node") == 0){
		noderequest(fp,clientSock,clntAddr);
	}
	else if(strcmp(buffer,"request:client") == 0){
		clientrequest(clientSock,clntAddr);
	}
	else{
		printf("Unknown request %s\n", buffer);
	}
}

void noderequest(FILE* fp, int nodeSock, struct sockaddr_in* nodeAddr){
	
	srand(time(NULL));

	// int node_accept_port = ntohs(nodeAddr->sin_port);
	int nodePort = ntohs(nodeAddr->sin_port) + rand()%1000;
	if(nodePort > 65535){
		nodePort = nodePort - 1000;
	}
	char nodeIP[INET_ADDRSTRLEN];
	memset(nodeIP,'\0',sizeof(nodeIP));
	if(inet_ntop(AF_INET, &(nodeAddr->sin_addr),nodeIP,sizeof(nodeIP)) == 0){
		DieWithError("inet_ntop() error");
	}
	// printf("Node Address@Port - %s@%d\n",nodeIP,nodePort);
	printf("Peer node %s:%d accepted\n",nodeIP,nodePort);

	StoreAddressPort(fp,nodeIP,nodePort);
	char* response = "response:server";
	char buffer[1024];
	memset(buffer,'\0',sizeof(buffer));

	sprintf(buffer,"%s\nstatus:connected\nport:%d",response,nodePort);

	int sentMsgsize;
	if((sentMsgsize = write(nodeSock,buffer,strlen(buffer))) < 0){
		DieWithError("noderequest() write() error");
	}
	sendMessagePrint(buffer);
	
	printf("Peer node %s:%d registered on the server\n",nodeIP,nodePort);
}

void clientrequest(int clientSock,struct sockaddr_in* clntAddr){
	
	int clientPort = ntohs(clntAddr->sin_port);

	char clientIP[INET_ADDRSTRLEN];
	memset(clientIP,'\0',sizeof(clientIP));
	if(inet_ntop(AF_INET, &(clntAddr->sin_addr),clientIP,sizeof(clientIP)) == 0){
		DieWithError("inet_ntop() error");
	}
	printf("Client %s:%d accepted\n",clientIP,clientPort);

	char* response = "response:server";
	char buffer[1024];
	memset(buffer,'\0',sizeof(buffer));

	sprintf(buffer,"%s\nstatus:connected\nport:%d",response,clientPort);

	int sentMsgsize,recvMsgSize;
	if((sentMsgsize = write(clientSock,buffer,strlen(buffer))) < 0){
		DieWithError("clientrequest() write() error");
	}
	sendMessagePrint(buffer);

	memset(buffer,'\0',sizeof(buffer));

	if((recvMsgSize = read(clientSock,buffer,sizeof(buffer) - 1)) < 0){
		DieWithError("clientrequest() read() error");
	}

	struct field* packet = packet_parser(buffer);
	char * ack = getfieldvalue(packet,"ack");

	if(strcmp(ack,"1")!=0){
		DieWithError("ack not recieved");
	}
	else{
        if(putfile(clientSock,NODESLIST) < 0){
            memset(buffer,'\0',sizeof(buffer));
            sprintf(buffer,"Failed to send peer nodes list to client %s:%d\n", clientIP,clientPort);
            DieWithError(buffer);
        }
        else{
        	printf("Peer nodes list sent to client %s:%d\n",clientIP,clientPort);
        }
    }

}

int putfile(int clientSock,char * filename){
    int recvMsgSize, sendMsgSize;
    
    int fd;
    struct stat file_stat;
    if((fd = open(filename,O_RDONLY)) < 0){
        return -1;
    }

    if(fstat(fd,&file_stat)<0){
        DieWithError("fstsat() error");
    }
    char buffer[1024];
    sprintf(buffer,"response:server\nfile:%s\nfilesize:%d",filename,(int)file_stat.st_size);
    if((sendMsgSize = write(clientSock,buffer,strlen(buffer))) < 0){
        DieWithError("write() error while sending filesize");
    }

    sendMessagePrint(buffer);
    memset(buffer,'\0',sizeof(buffer));

    if((recvMsgSize = read(clientSock,buffer,sizeof(buffer) - 1)) < 0){
        DieWithError("read() error");
    }

    struct field* packet = packet_parser(buffer);
    int n = atoi(packet[0].value);
    while(n>0 && strcmp(packet[--n].name,"ack")!=0);

    if(n<=0){
        DieWithError("No ack recieved");
    }
    off_t offset = 0,remain_data = file_stat.st_size;

    while((sendMsgSize = sendfile(clientSock,fd,&offset,BUFSIZ)) > 0 && remain_data){
        // printf("1. Server sent %d bytes from file's data and remaining data = %d\n", sendMsgSize, (int)remain_data);
        remain_data -= sendMsgSize;
        // printf("2. Server sent %d bytes from file's data and remaining data = %d\n", sendMsgSize, (int)remain_data);
        
        memset(buffer,'\0',sizeof(buffer));
        if((recvMsgSize = read(clientSock,buffer,sizeof(buffer) - 1)) < 0){
            DieWithError("read() error");
        }
        if(strcmp(getfieldvalue(packet_parser(buffer),"ack"),"1") != 0){
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

void StoreAddressPort(FILE* fp,char *IP, int port){
	fprintf(fp, "%s %d\n",IP,port);
}

void recvMessagePrint(char *s){
	// printf("Message recieved = [%s]\n", s);
}

void sendMessagePrint(char *s){
	// printf("Message sent = [%s]\n", s);
}
