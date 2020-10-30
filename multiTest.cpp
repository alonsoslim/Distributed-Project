/**
	Handle multiple socket connections with select and fd_set on Linux
*/

#include <stdio.h>
#include <string.h>   //strlen
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>   //close
#include <arpa/inet.h>    //close
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/time.h> //FD_SET, FD_ISSET, FD_ZERO macros
#include <time.h>

#define TRUE   1
#define FALSE  0
#define PORT 8888
const int MAX_PLAYERS = 10;

enum Role {
    Mafia = 1,
    Detective,
    Doctor,
    Civilian
};

class Player {
    public:
    int number;
    char name[30];
    Role role;
    bool hasRole = false;
    bool isDead = false;
    bool hasVoted = false;
};

class Global {
public:
	int client_socket[30];
	int nclients;
    int mafiaCount = 1;
    int detectCount = 1;
    int doctorCount = 1;
    int civCount = MAX_PLAYERS - mafiaCount
                - detectCount- doctorCount;
    Player player[MAX_PLAYERS];
	Global() {
		nclients = 0;
	}
} g;

void assignRoles(int playerNum);
void messageFromClient(int playerIdx, char *buffer);

int main(int argc , char *argv[])
{
    int opt = TRUE;
    int master_socket;
    int addrlen;
    int new_socket;
    int max_clients = 30;
    int activity;
    int i;
    int valread;
    int sd;
	int max_sd;
    char ts[1640];
    char *buff;
    char *p;
    struct sockaddr_in address;
    
    char buffer[1025];  //data buffer of 1K
    
    //set of socket descriptors
    fd_set readfds;
    
    //a message
    char *message = "ECHO Daemon v1.0 \r\n";

    //initialise all client_socket[] to 0 so not checked
    for (i = 0; i < max_clients; i++) 
    {
        g.client_socket[i] = 0;
    }
    
    //create a master socket
    if( (master_socket = socket(AF_INET , SOCK_STREAM , 0)) == 0) 
    {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    //set master socket to allow multiple connections , this is just a good habit, it will work without this
    if( setsockopt(master_socket, SOL_SOCKET, SO_REUSEADDR, (char *)&opt, sizeof(opt)) < 0 )
    {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }

    //type of socket created
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons( PORT );
    
    //bind the socket to localhost port 8888
    if (bind(master_socket, (struct sockaddr *)&address, sizeof(address))<0) 
    {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }
	printf("Listener on port %d \n", PORT);
	
    //try to specify maximum of 3 pending connections for the master socket
    if (listen(master_socket, 3) < 0)
    {
        perror("listen");
        exit(EXIT_FAILURE);
    }
    
    //accept the incoming connection
    addrlen = sizeof(address);
    puts("Waiting for connections ...");
    
	while(TRUE) 
    {
        //clear the socket set
        FD_ZERO(&readfds);

        //add master socket to set
        FD_SET(master_socket, &readfds);
        max_sd = master_socket;
		
        //add child sockets to set
        for ( i = 0 ; i < max_clients ; i++) 
        {
            //socket descriptor
			sd = g.client_socket[i];
            
			//if valid socket descriptor then add to read list
			if(sd > 0)
				FD_SET( sd , &readfds);
            
            //highest file descriptor number, need it for the select function
            if(sd > max_sd)
				max_sd = sd;
        }

        //wait for an activity on one of the sockets , timeout is NULL , so wait indefinitely
        activity = select( max_sd + 1 , &readfds , NULL , NULL , NULL);

        if ((activity < 0) && (errno!=EINTR)) 
        {
            printf("select error");
        }
        
        //If something happened on the master socket , then its an incoming connection
        if (FD_ISSET(master_socket, &readfds)) 
        {
            if ((new_socket = accept(master_socket, (struct sockaddr *)&address, (socklen_t*)&addrlen))<0)
            {
                perror("accept");
                exit(EXIT_FAILURE);
            }
        
            //inform user of socket number - used in send and receive commands
            printf("New connection , socket fd is %d , ip is : %s , port : %d \n" , new_socket , inet_ntoa(address.sin_addr) , ntohs(address.sin_port));

            //send new connection greeting message
            if( send(new_socket, message, strlen(message), 0) != strlen(message) ) 
            {
                perror("send");
            }
            
            puts("Welcome message sent successfully");
            
            //add new socket to array of sockets
            for (i = 0; i < max_clients; i++) 
            {
                //if position is empty
				if( g.client_socket[i] == 0 )
                {
                    g.client_socket[i] = new_socket;
                    printf("Adding to list of sockets as %d\n" , i);
                    g.nclients++;
					
					break;
                }
            }
        }
        
        //else its some IO operation on some other socket :)
        for (i = 0; i < g.nclients; i++) 
        {
            sd = g.client_socket[i];
            
            if (FD_ISSET( sd , &readfds)) 
            {
                //Check if it was for closing , and also read the incoming message
                if ((valread = read( sd , buffer, 1024)) == 0)
                {
                    //Somebody disconnected , get his details and print
                    getpeername(sd , (struct sockaddr*)&address , (socklen_t*)&addrlen);
                    printf("Host disconnected , ip %s , port %d \n" , inet_ntoa(address.sin_addr) , ntohs(address.sin_port));
                    
                    //Close the socket and mark as 0 in list for reuse
                    close( sd );
                    g.client_socket[i] = 0;
                }
                
                //Echo back the message that came in
                else
                {
                    buffer[valread] = '\0';
                    messageFromClient(i, buffer);
                    
                    /*
                    buff = buffer;
                    p = strstr(buff, "New Message:");
                    
                    //set the string terminating NULL byte on the end of the data read
                    //buffer[valread] = '\0';
                    //send(sd , buffer , strlen(buffer) , 0 );

                    char message[400] = "";
                    sprintf(message, "NEW MESSAGE\n");
                    int slen = strlen(message);
                    for (int j = 0; j < nclients; j++) {
                        if (client_socket[j] > 0)
                            send(client_socket[j], message, slen, 0);
                    }
                    buff = x + 3;
                    */
                }
            }
        }
    }
    
    return 0;
}

void messageFromClient(int playerIdx, char *buffer)
{
	//printf("messageFromClient(%i, **%s**)...\n", playerIdx, buffer);
	//Check for multiple messages in the buffer string.
	int k = playerIdx;
	char ts[1640];
	int done = 0;
	char *buff = buffer;
	char *p;
    int slen = 0;
	//any broadcast messages?
	while (!done) {
        done = 1;
        p = strstr(buff, "start");
        if (p) {
            printf("inside of start\n");
            assignRoles(g.nclients);
        }
        //printf("broadcast came in.\n");
        //broadcast:hello everyonex#zbroadcast:byex#z");
        //message will end with x#z
        sprintf(ts, "new message from %c: ", (char)'A'+k);	
        //char *x = strstr(p+12, "x#z");
        
        strcat(ts, buff);
        //printf("2\n");
        //Echo back the message that came in, to all players.
        int slen = strlen(ts);
        char *x = ts;
        *(x+slen) = '\0';
        //printf("sending **%s**\n", ts);
        for (int j=0; j<g.nclients; j++) {
            if (g.client_socket[j] > 0)
                send(g.client_socket[j], ts, slen, 0);
        }
        buff = x + slen;
        done = 1;
        //}
	}
}

void assignRoles(int playerNum) {
    //assign player roles
    //Mafia assign
    char ts[1640];
    int slen = 0;
    srand(time(0));
    g.civCount = playerNum - g.mafiaCount - g.detectCount - g.doctorCount;
    while (g.mafiaCount > 0) {
        int rnum = rand() % playerNum;
        if (g.player[rnum].hasRole == false) {
            g.player[rnum].role = Mafia;
            g.player[rnum].hasRole = true;
            g.mafiaCount--;
            sprintf(ts, "You are Mafia\n");
            slen = strlen(ts);
            send(g.client_socket[rnum], ts, slen, 0);
        }
    }
    //Detective assign
    while (g.detectCount > 0) {
        int rnum = rand() % playerNum;
        if (g.player[rnum].hasRole == false) {
            g.player[rnum].role = Detective;
            g.player[rnum].hasRole = true;
            g.detectCount--;
            sprintf(ts, "You are Detective\n");
            slen = strlen(ts);
            send(g.client_socket[rnum], ts, slen, 0);
        }
    }
    //Doctor assign
    while (g.doctorCount > 0) {
        int rnum = rand() % playerNum;
        if (g.player[rnum].hasRole == false) {
            g.player[rnum].role = Doctor;
            g.player[rnum].hasRole = true;
            g.doctorCount--;
            sprintf(ts, "You are Doctor\n");
            slen = strlen(ts);
            send(g.client_socket[rnum], ts, slen, 0);
        }
    }
    //Civilian assign
    while (g.civCount > 0) {
        int rnum = rand() % playerNum;
        if (g.player[rnum].hasRole == false) {
            g.player[rnum].role = Civilian;
            g.player[rnum].hasRole = true;
            g.civCount--;
            sprintf(ts, "You are Civilian\n");
            slen = strlen(ts);
            send(g.client_socket[rnum], ts, slen, 0);
        }
    }
    printf("ROLES ASSIGNED\n");
}