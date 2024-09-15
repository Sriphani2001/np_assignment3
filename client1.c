
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <regex.h>
#include <netdb.h>
#include <strings.h> 
#define LENGTH 2048
#define LENGTH2 12
#define DEBUG

//Initializing global variables
volatile sig_atomic_t flag = 0;
int sockfd = 0;
char name[32];
char username[2048];
 
void Overwrite_STDOUT() 
{
  fflush(stdout);
}
 
//sending out messages of not more than 256 characters
void Send_Message() 
{
  char message[2048] = {};
	char buffer[LENGTH + 32] = {};

  while(1) 
  {
  Overwrite_STDOUT();
  fgets(message, LENGTH, stdin);
  //check the message content and count the characters
  int charnum = strlen(message)-1;
    //The message should not exceed 256 
    if (charnum>255) 
    {
      printf("Maximum charachters reached you have:%d. resent shorter message\n",charnum);
      fflush(stdout);
    }
    else
    {
        //check protocol and send protocol to the server
        char Protocol_Msg[LENGTH]="MSG ";
        strcat(Protocol_Msg,message);
        strcat(Protocol_Msg,"\n");
        send(sockfd, Protocol_Msg, strlen(Protocol_Msg), 0);
        //clearing buffer
        bzero(message, LENGTH);
        bzero(buffer, LENGTH + 32);
    }
  }

}

// Receiving messages from server and splitting message into lines

void Recv_Message()
{
    char message[LENGTH] = {};
    
    while (1) 
    {
        // Receiving message from the server
        int receive = recv(sockfd, message, LENGTH, 0);
              //working on the protocol, username and message 
        char get_Protocol[LENGTH];
        char getUserName[LENGTH];
        char getMessage[LENGTH];

        if (receive > 0) 
        {
            char *line = strtok(message, "\n"); // Split message into lines

            while (line != NULL) 
            {
                // Process each line
                int rv = sscanf(line, "%s %s %[^\n]", get_Protocol, getUserName, getMessage);
                char dest[LENGTH] = {0};
                memcpy(dest, getMessage, sizeof(getMessage));

                // Check the message prefix and compare it with the username
                if (strcmp(get_Protocol, "MSG") == 0) 
                {
                    if (strcmp(getUserName, username) != 0) 
                    {
                        printf("%s\n", dest); // Print each line
                        fflush(stdout);
                    }
                }

                // Get the next line
                line = strtok(NULL, "\n");
            }
        } else if (receive == 0) 
        {
            break;
        }

        memset(message, 0, sizeof(message));
    }
}

//main function 
int main(int argc, char *argv[])
{
//check CLI arguments 
	if (argc != 3) 
    {
        printf("Please provide exactly three arguments: IP:Port and a nickname/number.\n");
        fflush(stdout);
        exit(1); // Exit with a non-zero status code to indicate an error.
    } 
    else 
    {
        char *ipPortArg = argv[1];
        char *colon = strchr(ipPortArg, ':');
        if (colon == NULL) 
        {
            printf("Error: IP and Port must be separated by a colon (e.g., 127.0.0.1:8080).\n");
            fflush(stdout);
            exit(1); // Exit with an error status code.
        }
    }

// checking the  nicknames
  char *expression="^[A-Za-z0-9_]+$", *org ;
   regex_t regularexpression;
  int reti;
  int matches;
  regmatch_t items;

  reti=regcomp(&regularexpression, expression,REG_EXTENDED);
  if(reti)
  {
    printf("Could not compile regex.\n");
    fflush(stdout);
    exit(0);
  }

for (int i = 2; i < argc; i++) 
{
    if (strlen(argv[i]) <= 12) 
    { 
        reti = regexec(&regularexpression, argv[i], matches, &items, 0);
        if (!reti) 
        {
            // Clearing username, check for validity using regex
            bzero(username, LENGTH);
            strncpy(username, argv[i], LENGTH);
            #ifdef DEBUG
            printf("%s is valid User Name.\n", argv[i]);
            fflush(stdout);
            #endif
        } else 
        {
            // Error if the username is not valid
            printf("ERROR, %s is NOT valid User Name.\n", argv[i]);
            fflush(stdout);
            exit(1); // Exit with a non-zero status code to indicate an error
        }
    } else 
    {
        // The nickname should be less than or equal to 12 characters
        printf(" ERROR %s is too long (%zu vs 12 chars).\n", argv[i], strlen(argv[i]));
        fflush(stdout);
        exit(1); // Exit with a non-zero status code to indicate an error
    }
}

  regfree(&regularexpression);
  free(org);

//Declaration of variables 
  char *hoststring,*portstring, *rest;
  struct addrinfo hints;
  struct addrinfo* res=0;

//GET name and port from arguments
  org=strdup(argv[1]);
  rest=argv[1];
  hoststring=strtok_r(rest,":",&rest);
  portstring=strtok_r(rest,":",&rest);
  int port=atoi(portstring);
  printf("Connected to %s:%d \n",hoststring,port);
  fflush(stdout);

// get the address information from the server 
	memset(&hints,0,sizeof(hints));
	hints.ai_family=AF_UNSPEC;
	hints.ai_socktype=SOCK_STREAM;
	hints.ai_protocol=0;
	hints.ai_flags=AI_ADDRCONFIG;
	int err=getaddrinfo(hoststring,portstring,&hints,&res);
	if (err!=0) 
	{
    printf("ERROR in getaddrinfo\n");
    fflush(stdout);
    exit(0);
	}
// Creating the socket
	sockfd=socket(res->ai_family,res->ai_socktype,res->ai_protocol);
	if (sockfd==-1) 
	{
		printf("ERROR in socket creation\n");
    fflush(stdout);
		exit(0);
	}
// Connecting to server
	if (connect(sockfd,res->ai_addr,res->ai_addrlen)==-1) 
	{
    printf("ERROR in SOCKET Connetion\n");
    fflush(stdout);
    exit(0);
	}
//Get the server Protocol
  char Protocol[LENGTH];
  bzero(Protocol, LENGTH);
  int n = read(sockfd, Protocol, sizeof(Protocol));
  if (n <= 0) 
  {
    perror("ERROR reading Server Protocol\n");
    fflush(stderr);	
    exit(0);	
  }
  printf("Serve Protocol: %s", Protocol);
  fflush(stdout);
//check the server protocol and proceed 
  if (strcmp(Protocol,"HELLO 1\n")==0)
  {
    char NICProtocolMsg[LENGTH]="NICK ";
    strcat(NICProtocolMsg,username);
    strcat(NICProtocolMsg,"\n");
    printf("sending NICK name to server:%s\n",NICProtocolMsg);
    fflush(stdout);

    #ifdef DEBUG
    printf("Protocol Supported, Sending Nick Name to server\n");
    fflush(stdout);
    #endif

    if(send(sockfd, NICProtocolMsg, strlen(NICProtocolMsg), 0)<=0)
    {
      perror("ERROR Sending UserName to server\n");
      fflush(stderr);
      exit(0);
    }
    else
    {
      char response[LENGTH]={0};
      int n = read(sockfd, response, sizeof(response));
      if (n <= 0) 
      {
        printf("ERROR reading Server response\n");	
        fflush(stdout);
        exit(0);	
      }
      else
      {
        #ifdef DEBUG
	printf("Server message : %s\n",response);
        fflush(stdout);
			  #endif

        if(strcmp(response,"OK\n")==0)
        {
            printf("NICKName Accepted \n",username);
            fflush(stdout);
        }
        else
        {
            printf("ERROR, NICKName NOT Accepted  \n",username);
            fflush(stdout);
            exit(0);
        }
      }
    }
  }
  else
  {
  printf("ERROR Server Protocl NOT support\n");
  fflush(stdout);
  exit(0);
  }
  
  printf("WELCOME THE CHAT \n");
  fflush(stdout);

   pthread_t send_thread;
  if(pthread_create(&send_thread, NULL, (void *) Send_Message, NULL) != 0)
  {
    printf("ERROR: pthread\n");
    fflush(stdout);
    return 1;
	}

   pthread_t receive_thread;
  if(pthread_create(&receive_thread, NULL, (void *) Recv_Message, NULL) != 0)
  {
    printf("ERROR: check on the pthread\n");
    fflush(stdout);
    return 1;
	}

while (1)
  {
		if(flag)
    {
			printf("\n BYE BYE, EXITING \n");
      fflush(stdout);
			break;
    }
	}
	    close(sockfd);
  return 0;
}

//////////////
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include <sys/types.h>
#include <signal.h>
#include <sys/types.h>
#include <ifaddrs.h>
#include <unistd.h>
#include <math.h>
#include <netdb.h>
#include <regex.h>

#define MAX_CLIENTS 50
#define maxbuffersize 2048
// #define DEBUG

static _Atomic unsigned int cli_count = 0;
static int uid = 10;
int leave_flag = 0;

// Client structure 
typedef struct
{
	struct sockaddr_in address;
	int sockfd;
	int uid;
	char name[12];
} client_t;

client_t *clients[MAX_CLIENTS];
pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER;

// SECTION ==Add clients to the list to chat with others
void queue_add(client_t *cl)
{
	pthread_mutex_lock(&clients_mutex);

	for(int i=0; i < MAX_CLIENTS; ++i)
    {
		if(!clients[i])
        {
			clients[i] = cl;
			break;
		}
	}
	pthread_mutex_unlock(&clients_mutex);
}

// SECTION ==Remove clients from the list after completing the chat
void queue_remove(int uid)
{
	pthread_mutex_lock(&clients_mutex);
	for(int i=0; i < MAX_CLIENTS; ++i)
    {
		if(clients[i])
        {
			if(clients[i]->uid == uid)
            {
				clients[i] = NULL;
				break;
			}
		}
	}
	pthread_mutex_unlock(&clients_mutex);
}

//How to send messages to all clients
void send_message(char *s, int uid)
{
	pthread_mutex_lock(&clients_mutex);
	for(int i=0; i<MAX_CLIENTS; ++i)
    {
		if(clients[i])
        {
			if(clients[i]->uid != uid)
            {
				if(send(clients[i]->sockfd, s, strlen(s),0) <= 0)
                {
					printf("ERROR: writing to sockedfd  failed");
					fflush(stdout);
					break;
				}
			}
		}
	}
	pthread_mutex_unlock(&clients_mutex);
}

// Function for handling all communications with the client 

void *handle_client(void *arg)
{
	char mem1[maxbuffersize];
	int leave_flag = 0;

	cli_count++;
	client_t *cli = (client_t *)arg;

	while(1)
    {
		if (leave_flag) 
        {
			break;
		}
		int receive = recv(cli->sockfd, mem1, maxbuffersize, 0);
		if (receive > 0)
        {
			if(strlen(mem1) > 0)
            {
				char message [maxbuffersize]={0};
				char out_message [maxbuffersize]={0};
				char mem1_out [maxbuffersize]={0};
				
				memcpy(message,mem1+3,sizeof(mem1));
				message[strcspn(message, "\n")] = '\0';

				memcpy(mem1_out,mem1+3,sizeof(mem1));
				//check the ending creterion , either a \n or a space \r
	
					if (mem1_out[receive-2] == 0x0d)
					{
						mem1_out[receive-2] = 0;
					} 
					else if (mem1_out[receive-1] == '\n')
					{
						mem1_out[receive-1] = 0;
					}

				//checking message character length 255 
			  
    			int charcount = strlen(message)-1;
				if(charcount<257)
				{
					sprintf(out_message,"%s %s %s%s","MSG",cli->name,message,"\n");
					printf("%s %s",cli->name,mem1_out);
					fflush(stdout);
					send_message(out_message, cli->uid);
				}
 				else
				{
					sprintf(mem1, "ERROR %s handle , for character \n", cli->name);
					send_message(mem1, cli->name);
					//leave_flag = 1;	
				} 

			}
		} 
        else if (receive == 0)
        {
			//If a client leaves, the server has to update  person
			sprintf(mem1, "%s left \n", cli->name);
			printf("%s", mem1);
			fflush(stdout);
			
			leave_flag = 1;
		} 
		//checking message on receive
		else if (receive<0)
		{
			sprintf(mem1, "ERROR %s handle , for character, possible no message ---check\n", cli->name);
			
			leave_flag = 1;
		}
        else 
        {
			//checking error in the communication
			printf("ERROR: -1\n");
			fflush(stdout);
			leave_flag = 1;
		}
		//clear memory
		bzero(mem1,maxbuffersize);	
	}

  // when to remove clients from queues
	close(cli->sockfd);
	queue_remove(cli->uid);
	free(cli);
	cli_count--;
	pthread_detach(pthread_self());
	return NULL;
}
// main function 

int main(int argc, char **argv)
{
	//check the CLI arguments 
	if(argc != 2)
	{
		printf("ERROR Please enter ip and port in ths order  IP: port\n");
		fflush(stdout);
		exit(0);
	}
	//get the values from CLI
	char delim[]=":";
 	char *Desthost=strtok(argv[1],delim);
  	char *Destport=strtok(NULL,delim);
	int port=atoi(Destport);
	printf("Host %s, and port %d.\n",Desthost,port);
	fflush(stdout);

	// assign IP, PORT
	struct addrinfo hints;
	memset(&hints,0,sizeof(hints));
	hints.ai_family=AF_UNSPEC;
	hints.ai_socktype=SOCK_STREAM;
	hints.ai_protocol=0;
	hints.ai_flags=AI_ADDRCONFIG;
	struct addrinfo* res=0;
	int err=getaddrinfo(Desthost,Destport,&hints,&res);
	if (err!=0) 
	{
    		perror("ERROR Failed to resolve remote socket address\n");
			fflush(stderr);
    		exit(0);
	}
	//Initializing variables
	int sockfd;
	int option = 1;
	pthread_t tid;
	int connfd = 0;
	struct sockaddr_in serv_addr;
	struct sockaddr_in cli_addr;
	char name[12]={0};
  
	// socket creation and verification (server supports IPV4 and TCP only)
	sockfd = socket(res->ai_family,res->ai_socktype,res->ai_protocol);
	if(sockfd<=0)
	{
		printf("ERROR Server socket creation failed\n");
		fflush(stdout);
		exit(0);
	}
	else
		printf("Socket successfully created\n");
		fflush(stdout);

	if(setsockopt(sockfd, SOL_SOCKET,(SO_REUSEPORT | SO_REUSEADDR),(char*)&option,sizeof(option)) < 0)
	{
		printf("ERROR setsockopt failed\n");
		fflush(stdout);
    	exit(0);
	}
	// Binding socket to server IP
	if ((bind(sockfd, res->ai_addr,res->ai_addrlen)) != 0) 
	{
		printf("ERROR Server socket bind failed\n");
		fflush(stdout);
		exit(0);
	}
	else
		printf("Socket successfully binded..\n");
		fflush(stdout);
	// server listening to a number of clients ===50
	if ((listen(sockfd, 50)) != 0) 
	{
		printf("50 client allowed, please chceck..\n");
		fflush(stdout);
		exit(0);
	}
	else
		printf("Server listening for incominf conncetion..\n");
		fflush(stdout);

	freeaddrinfo(res);
	printf("<<<<<<<WELCOME TO CHAT SYSTEM >>>>>>>\n");
	fflush(stdout);

	while(1)
	{
		socklen_t clilen = sizeof(cli_addr);
		connfd = accept(sockfd, (struct sockaddr*)&cli_addr, &clilen);

		// Check if max clients is reached
		if((cli_count + 1) == MAX_CLIENTS)
		{
			printf("ERROR, Rejected: cleint maxnumber reachead\n ");
			fflush(stdout);
			printf(":%d\n", cli_addr.sin_port);
			fflush(stdout);
			close(connfd);
			continue;
		}
		//sending the version protocol
		char Protocol[maxbuffersize]="HELLO 1\n";
		if(send(connfd, Protocol, strlen(Protocol), 0)<=0)
		{
			printf("ERROR, WRONG PROTOCOL VERSION \n");
			fflush(stdout);
			exit(0);
		}
		
		//NICK Name check
		char NICKmessage[maxbuffersize]={0};
		if(recv(connfd, NICKmessage, 32, 0) <= 0)
	    {
			printf("ERROR Receiving  NICK Name message\n");
			fflush(stdout);
			leave_flag = 1;
		} 
	    else
	    {		
			memcpy(name, NICKmessage+5,sizeof(NICKmessage));
			name[strcspn(name, "\n")] = '\0';
			#ifdef DEBUG
			printf("NICK Name received:%s\n",name);
			fflush(stdout);
			#endif

			//checking the NICK name acceptance 
			char *expression="^[A-Za-z0-9_]+$";
			regex_t regularexpression;
			int reti;
			reti=regcomp(&regularexpression, expression,REG_EXTENDED);
			if(reti)
			{
				printf("ERROR, Could not compile regex.\n");
				fflush(stdout);
				exit(0);
			}
			int matches;
			regmatch_t items;
		
			if(strlen(name)<12)
			{
				reti=regexec(&regularexpression, name,matches,&items,0);
				if(!reti)
				{
					//sending the OK to the client 
					char OKmessage[12]="OK\n";
					if(send(connfd, OKmessage, strlen(OKmessage),0)<=0)
					{
						printf("ERROR on sending OK message");
						fflush(stdout);
					}
					else
					{					
						//adding new client to the list
						client_t *cli = (client_t *)malloc(sizeof(client_t));
						cli->address = cli_addr;
						cli->sockfd = connfd;
						cli->uid = uid++;
						strcpy(cli->name, name);
						printf("%s JOINED THE CHAT\n",cli->name);
						fflush(stdout);
						queue_add(cli);
						pthread_create(&tid, NULL, &handle_client, (void*)cli);
					}
				} 
				else 
				{
					//sending the ERROR to client when not allowed to the chat
					char Errormessage[12]="error joining chat\n";
					if(send(connfd, Errormessage, strlen(Errormessage),0)<=0)
					{
						printf("ERROR on sending NICK \n");
						fflush(stdout);
					}	
				}
			} 
			else
			{
				// When there are more than 12
				char Errormessage[12]="ERROR\n";
				if(send(connfd, Errormessage, strlen(Errormessage),0)<=0)
				{
					printf("ERROR on sending NICK");
					fflush(stdout);
				}
			}
		}
	}

	return 0;
}