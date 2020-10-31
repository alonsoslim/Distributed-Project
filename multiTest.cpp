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
    int mafiaCount = 2;
    int detectCount = 1;
    int doctorCount = 1;
    int mafia[2];
    int detect[1];
    int doctor[1];
    int dead[MAX_PLAYERS];
    int deadCount = 0;
    int currentDead = -1;
    int currentSuspect = -1;
    int currentSave = -1;
    int civCount = MAX_PLAYERS - mafiaCount
                - detectCount- doctorCount;
    Player player[MAX_PLAYERS];
    bool isStart = false;
    int currentAction = 0;
	Global() {
		nclients = 0;
	}
} g;

void sendAll(char *message);
void sendMafia();
void sendDetect();
void sendDoctor();
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
    int slen = 0;
    //char *buff;
    //char *p;
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
                    sprintf(ts, "You are Player: %c\n", (char)'A'+i);
                    slen = strlen(ts);
                    send(g.client_socket[i], ts, slen, 0);
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
                    if (g.currentAction == Mafia)
                    {
                        for (int j = 0; j < g.mafiaCount; j++)
                        {
                            if (i == g.mafia[j])
                            {
                                buffer[valread] = '\0';
                                messageFromClient(i, buffer);
                            }
                        }
                    }
                    else if (g.currentAction == Detective)
                    {
                        for (int j = 0; j < g.detectCount; j++)
                        {
                            if (i == g.detect[j])
                            {
                                buffer[valread] = '\0';
                                messageFromClient(i, buffer);
                            }
                        }
                    }
                    else if (g.currentAction == Doctor)
                    {
                        for (int j = 0; j < g.doctorCount; j++)
                        {
                            if (i == g.doctor[j])
                            {
                                buffer[valread] = '\0';
                                messageFromClient(i, buffer);
                            }
                        }
                    }
                    else if (g.currentAction == Civilian)
                    {
                        buffer[valread] = '\0';
                        messageFromClient(i, buffer);
                    }
                    else
                    {
                        buffer[valread] = '\0';
                        messageFromClient(i, buffer);
                    }
                }
            }
        }
    }
    
    return 0;
}

void sendAll(char *message)
{
    int slen = strlen(message);
    for (int i = 0; i < g.nclients; i++)
    {
        if (g.client_socket[i] > 0)
            send(g.client_socket[i], message, slen, 0);
    }
}

void sendMafia()
{
    printf("start of mafia\n");
    char ts[1640];
    int slen = 0;
    for (int i = 0; i < g.mafiaCount; i++)
    {
        sprintf(ts, "Choose someone to kill.\n");
        slen = strlen(ts);
        send(g.client_socket[g.mafia[i]], ts, slen, 0);
    }
    g.currentAction = Mafia;
    printf("end of mafia\n");
}

void sendDetect()
{
    char ts[1640];
    int slen = 0;
    for (int i = 0; i < g.detectCount; i++)
    {
        sprintf(ts, "Choose someone to suspect.\n");
        slen = strlen(ts);
        send(g.client_socket[g.detect[i]], ts, slen, 0);
    }
    g.currentAction = Detective;
}

void sendDoctor()
{
    char ts[1640];
    int slen = 0;
    for (int i = 0; i < g.doctorCount; i++)
    {
        sprintf(ts, "Choose someone to save.\n");
        slen = strlen(ts);
        send(g.client_socket[g.doctor[i]], ts, slen, 0);
    }
    g.currentAction = Doctor;
}

void roundCheck()
{
    if (g.currentDead == g.currentSaved)
    {
        g.currentDead = -1;
    }
    else
    {
        //g.player[g.currentDead].isDead = true;
        g.dead[g.current] = g.currentDead;
        g.deadCount++;
        sendAll("Player %s has died.\n", (char'A'+g.currentDead));
    }
}

void messageFromClient(int playerIdx, char *buffer)
{
	//Check for multiple messages in the buffer string.
	int k = playerIdx;
	char ts[1640];
	int done = 0;
	char *buff = buffer;
	char *p;
    int slen = 0;
	//any broadcast messages?
	while (!done)
    {   
        if (g.currentAction == 0)
        {
            done = 1;
            p = strstr(buff, "start");
            if (p && !g.isStart)
            {
                printf("inside of start\n");
                assignRoles(g.nclients);
                sendAll("The game has begun\n");
                g.isStart = true;
                sendMafia();
                printf("current action is: %d\n", g.currentAction);
            }
            else
            {
                sprintf(ts, "new message from %c: ", (char)'A'+k);	
                strcat(ts, buff);
                //Echo back the message that came in, to all players.
                slen = strlen(ts);
                char *x = ts;
                *(x+slen) = '\0';
                for (int i = 0; i < g.nclients; i++)
                {
                    if (g.client_socket[i] > 0)
                        send(g.client_socket[i], ts, slen, 0);
                }
                buff = x + slen;
                done = 1;
            }
        }
        else if (g.currentAction == Mafia)
        {
            //printf("\nINSIDE kill check\ncurrent action is: %d\n", g.currentAction);
            if (strncmp(buff,"a",1) == 0)
            {
                g.currentDead = 0;
            }
            else if (strncmp(buff,"b",1) == 0)
            {
                g.currentDead = 1;
            }
            else if (strncmp(buff,"c",1) == 0)
            {
                g.currentDead = 2;
            }
            else if (strncmp(buff,"d",1) == 0)
            {
                g.currentDead = 3;
            }
            else if (strncmp(buff,"e",1) == 0)
            {
                g.currentDead = 4;
            }
            g.currentAction++;
            sendDetect();
            done = 1;
            //printf("current dead is: %d\n", g.currentDead);
        }
        else if (g.currentAction == Detective)
        {
            //printf("\nINSIDE suspect check\ncurrent action is: %d\n", g.currentAction);
            if (strncmp(buff,"a",1) == 0)
            {
                g.currentSuspect = 0;
            }
            else if (strncmp(buff,"b",1) == 0)
            {
                g.currentSuspect = 1;
            }
            else if (strncmp(buff,"c",1) == 0)
            {
                g.currentSuspect = 2;
            }
            else if (strncmp(buff,"d",1) == 0)
            {
                g.currentSuspect = 3;
            }
            else if (strncmp(buff,"e",1) == 0)
            {
                g.currentSuspect = 4;
            }
            g.currentAction++;
            sendDoctor();
            done = 1;
        }
        else if (g.currentAction == Doctor)
        {
            //printf("\nINSIDE save check\ncurrent action is: %d\n", g.currentAction);
            if (strncmp(buff,"a",1) == 0)
            {
                g.currentSave = 0;
            }
            else if (strncmp(buff,"b",1) == 0)
            {
                g.currentSave = 1;
            }
            else if (strncmp(buff,"c",1) == 0)
            {
                g.currentSave = 2;
            }
            else if (strncmp(buff,"d",1) == 0)
            {
                g.currentSave = 3;
            }
            else if (strncmp(buff,"e",1) == 0)
            {
                g.currentSave = 4;
            }
            g.currentAction++;
            roundCheck();
            done = 1;
        }
        //printf("\nOUTSIDE action checks\ncurrent action is: %d\n", g.currentAction);
        //printf("current dead is: %d\n", g.currentDead);
	}
}

void assignRoles(int playerNum)
{
    //assign player roles
    
    int tempCount = 0;
    char ts[1640];
    char st[1640];
    int slen = 0;
    srand(time(0));
    g.civCount = playerNum - g.mafiaCount - g.detectCount - g.doctorCount;
    //Mafia assign
    int tempMafia = g.mafiaCount;
    while (tempMafia > 0)
    {
        int rnum = rand() % playerNum;
        if (g.player[rnum].hasRole == false)
        {
            g.player[rnum].role = Mafia;
            g.player[rnum].hasRole = true;
            tempMafia--;
            sprintf(ts, "You are Mafia\n");
            slen = strlen(ts);
            send(g.client_socket[rnum], ts, slen, 0);
        }
    }
    
    //Finding all mafia
    for (int i = 0; i < playerNum; i++)
    {
        if (g.player[i].role == Mafia)
        {
            g.mafia[tempCount] = i;
            tempCount++;
        }
    }

    //Formatting string
    for (int i = 0; i < tempCount; i++)
    {
        if (tempCount == 1)
        {
            sprintf(ts, "You are the only Mafia");
        }
        else if (tempCount > 1 && i == 0)
        {
            sprintf(ts, "Mafia are: %c", (char)'A' + g.mafia[i]);
        }
        else if(tempCount > 1 && i > 0)
        {
            sprintf(st, " and %c", (char)'A' + g.mafia[i]);
            strcat(ts, st);
        }
    }
    sprintf(st, "\n");
    strcat(ts, st);
    slen = strlen(ts);

    //Sending who is mafia to all mafia
    for (int i = 0; i < tempCount; i++)
    {
        send(g.client_socket[g.mafia[i]], ts, slen, 0);
    }

    tempCount = 0;
    //Detective assign
    int tempDetect = g.detectCount;
    while (tempDetect > 0)
    {
        int rnum = rand() % playerNum;
        if (g.player[rnum].hasRole == false)
        {
            g.player[rnum].role = Detective;
            g.player[rnum].hasRole = true;
            tempDetect--;
            sprintf(ts, "You are Detective\n");
            slen = strlen(ts);
            send(g.client_socket[rnum], ts, slen, 0);
        }
    }

    //Finding all detective
    for (int i = 0; i < playerNum; i++)
    {
        if (g.player[i].role == Detective)
        {
            g.detect[tempCount] = i;
            tempCount++;
        }
    }

    //Formatting string
    for (int i = 0; i < tempCount; i++)
    {
        if (tempCount == 1)
        {
            sprintf(ts, "You are the only Detective");
        }
        else if (tempCount > 1 && i == 0)
        {
            sprintf(ts, "Detectives are: %c", (char)'A' + g.detect[i]);
        }
        else if(tempCount > 1 && i > 0)
        {
            sprintf(st, " and %c", (char)'A' + g.detect[i]);
            strcat(ts, st);
        }
    }
    sprintf(st, "\n");
    strcat(ts, st);
    slen = strlen(ts);

    //Sending who is detective to all detectives
    for (int i = 0; i < tempCount; i++)
    {
        send(g.client_socket[g.detect[i]], ts, slen, 0);
    }
    
    tempCount = 0;
    //Doctor assign
    int tempDoctor = g.doctorCount;
    while (tempDoctor > 0)
    {
        int rnum = rand() % playerNum;
        if (g.player[rnum].hasRole == false)
        {
            g.player[rnum].role = Doctor;
            g.player[rnum].hasRole = true;
            tempDoctor--;
            sprintf(ts, "You are Doctor\n");
            slen = strlen(ts);
            send(g.client_socket[rnum], ts, slen, 0);
        }
    }

    //Finding all Doctors
    for (int i = 0; i < playerNum; i++)
    {
        if (g.player[i].role == Doctor)
        {
            g.doctor[tempCount] = i;
            tempCount++;
        }
    }

    //Formatting string
    for (int i = 0; i < tempCount; i++)
    {
        if (tempCount == 1)
        {
            sprintf(ts, "You are the only Doctor");
        }
        else if (tempCount > 1 && i == 0)
        {
            sprintf(ts, "Doctors are: %c", (char)'A' + g.doctor[i]);
        }
        else if(tempCount > 1 && i > 0)
        {
            sprintf(st, " and %c", (char)'A' + g.doctor[i]);
            strcat(ts, st);
        }
    }
    sprintf(st, "\n");
    strcat(ts, st);
    slen = strlen(ts);

    //Sending who is doctor to all doctors
    for (int i = 0; i < tempCount; i++)
    {
        send(g.client_socket[g.doctor[i]], ts, slen, 0);
    }
    
    tempCount = 0;
    //Civilian assign
    int tempCiv = g.civCount;
    while (tempCiv > 0)
    {
        int rnum = rand() % playerNum;
        if (g.player[rnum].hasRole == false)
        {
            g.player[rnum].role = Civilian;
            g.player[rnum].hasRole = true;
            tempCiv--;
            sprintf(ts, "You are Civilian\n");
            slen = strlen(ts);
            send(g.client_socket[rnum], ts, slen, 0);
        }
    }
    printf("ROLES ASSIGNED\n");
}