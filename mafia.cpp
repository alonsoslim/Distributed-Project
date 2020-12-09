//Github

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
    int voteCount = 0;
};

class Global {
public:
	int client_socket[30];
	int nclients;
    int playerCount = 0;
    int mafiaCount = 1;
    int deadMafia = 0;
    int detectCount = 1;
    int doctorCount = 1;
    int mafia[1];
    int detect[1];
    int doctor[1];
    int deadCount = 0;
    int currentDead = -1;
    int currentSuspect = -1;
    int currentSave = -1;
    int civCount = MAX_PLAYERS - mafiaCount
                - detectCount- doctorCount;
    Player player[MAX_PLAYERS];
    bool isStart = false;
    int currentAction = 0;
    int totalVotes = 0;
    int votedPlayer = -1;
    bool isTie = false;
	Global() {
		nclients = 0;
	}
} g;

void log(char *message);
void sendAll(char *message);
void sendAllAlive(char *message);
void startMafia();
void mafiaChoiceConfirm(int choice);
void startDetect();
void detectChoiceConfirm(int choice);
void startDoctor();
void doctorChoiceConfirm(int choice);
void roundCheck();
bool voteCheck();
bool checkWin();
void gameOver();
void whosAlive();
void whosAlive(int role);
void playerStatus();
void messageFromClient(int playerIdx, char *buffer);
void assignRoles(int playerNum);

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
    struct sockaddr_in address;

    char buffer[1025];  //data buffer of 1K

    //set of socket descriptors
    fd_set readfds;

    //a message
    char *message = "Weclome to MAFIA\r\n";

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
            printf("New connection , socket fd is %d , ip is : %s , port : %d \n", new_socket, inet_ntoa(address.sin_addr), ntohs(address.sin_port));

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

        //else its some IO operation on some other socket
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
                    if (g.player[i].hasRole == true)
                    {
                        g.player[i].hasRole = false;
                        g.player[i].isDead = true;
                        g.deadCount++;
                        close( sd );
                        g.client_socket[i] = 0;
                        if (g.player[i].role == Mafia)
                        {
                            g.deadMafia++;
                            if (g.deadMafia >= g.mafiaCount)
                                checkWin();
                        }

                        if (g.player[i].role == Detective)
                            g.detectCount--;

                        if (g.player[i].role == Doctor)
                            g.doctorCount--;

                        if (g.player[i].role == Civilian)
                            g.civCount--;
                    }
                    else
                    {
                        //Close the socket and mark as 0 in list for reuse
                        close( sd );
                        g.client_socket[i] = 0;
                    }
                }
                //Read the message that came in
                else
                {
                    if (g.currentAction == Mafia)
                    {
                        for (int j = 0; j < g.mafiaCount; j++)
                        {
                            if (i == g.mafia[j] && g.player[i].isDead == false)
                            {
                                printf("Message received from mafia: %c\n", (char)'A'+i);
                                buffer[valread] = '\0';
                                messageFromClient(i, buffer);
                            }
                        }
                    }
                    else if (g.currentAction == Detective)
                    {
                        for (int j = 0; j < g.detectCount; j++)
                        {
                            if (i == g.detect[j] && g.player[i].isDead == false)
                            {
                                printf("Message received from dectective: %c\n", (char)'A'+i);
                                buffer[valread] = '\0';
                                messageFromClient(i, buffer);
                            }
                        }
                    }
                    else if (g.currentAction == Doctor)
                    {
                        for (int j = 0; j < g.doctorCount; j++)
                        {
                            if (i == g.doctor[j] && g.player[i].isDead == false)
                            {
                                printf("Message received from doctor: %c\n", (char)'A'+i);
                                buffer[valread] = '\0';
                                messageFromClient(i, buffer);
                            }
                        }
                    }
                    else if (g.currentAction == Civilian)
                    {
                        if (g.player[i].isDead == false && g.player[i].hasVoted == false
                        && g.player[i].hasRole == true)
                        {
                            printf("Vote received from: %c\n", (char)'A'+i);
                            buffer[valread] = '\0';
                            messageFromClient(i, buffer);
                        }
                    }
                    else
                    {
                        printf("Message received from: %c\n", (char)'A'+i);
                        buffer[valread] = '\0';
                        messageFromClient(i, buffer);
                    }
                }
            }
        }
    }
    return 0;
}

//=============================================================================
//Game Logic Functions
//=============================================================================

void sendAll(char *message)
{
    log("start of sendAll\n");
    int slen = strlen(message);
    for (int i = 0; i < g.nclients; i++)
    {
        if (g.client_socket[i] > 0)
        {
            send(g.client_socket[i], message, slen, 0);
        }
    }
    log("end of sendAll\n");
}

void sendAllAlive(char *message)
{
    log("start of sendAllAlive\n");
    int slen = strlen(message);
    for (int i = 0; i < g.playerCount; i++)
    {
        if (g.client_socket[i] > 0 && g.player[i].isDead == false)
            send(g.client_socket[i], message, slen, 0);
    }
    log("end of sendAllAlive\n");
}

//=============================================================================
//Mafia Functions
//=============================================================================

void startMafia()
{
    log("start of startMafia\n");
    char ts[1640];
    int slen = 0;
    int tempMafia = g.mafiaCount;
    for (int i = 0; i < tempMafia; i++)
    {
        if (g.player[g.mafia[i]].isDead == true)
            tempMafia++;
        else
        {
            sprintf(ts, "\nChoose someone to kill.\n");
            slen = strlen(ts);
            if (g.client_socket[g.mafia[i]] > 0)
                send(g.client_socket[g.mafia[i]], ts, slen, 0);
        }
    }
    whosAlive(Mafia);
    g.currentAction = Mafia;
    printf("current action: %d\n", g.currentAction);
    log("end of startMafia\n");
}

void mafiaChoiceConfirm(int choice)
{
    log("start of mafiaChoiceConfirm\n");
    char ts[1640];
    int slen = 0;
    int tempMafia = g.mafiaCount;
    for (int i = 0; i < tempMafia; i++)
    {
        if (g.player[g.mafia[i]].isDead == true)
            tempMafia++;
        else
        {
            sprintf(ts, "You chose player: %c\n", (char)'A'+choice);
            slen = strlen(ts);
            if (g.client_socket[g.mafia[i]] > 0)
                send(g.client_socket[g.mafia[i]], ts, slen, 0);
        }
    }
    log("end of mafiaChoiceConfirm\n");
}

//=============================================================================
//Detective Functions
//=============================================================================

void startDetect()
{
    log("start of startDetect\n");
    char ts[1640];
    int slen = 0;
    int tempDetect;
    for (int i = 0; i < g.detectCount; i++)
    {
        if (g.player[g.detect[i]].isDead == true)
            tempDetect++;
        else
        {
            sprintf(ts, "\nChoose someone to suspect.\n");
            slen = strlen(ts);
            if (g.client_socket[g.detect[i]] > 0)
                send(g.client_socket[g.detect[i]], ts, slen, 0);
        }
    }
    whosAlive(Detective);
    g.currentAction = Detective;
    printf("current action: %d\n", g.currentAction);
    log("end of startDetect\n");
}

void detectChoiceConfirm(int choice)
{
    log("start of detectChoiceConfirm\n");
    char ts[1640];
    int slen = 0;
    int tempDetect = g.detectCount;
    for (int i = 0; i < tempDetect; i++)
    {
        if (g.player[g.detect[i]].isDead == true)
            tempDetect++;
        else
        {
            if (g.player[choice].role == Mafia)
            {
                sprintf(ts, "Player: %c is mafia...\n", (char)'A'+choice);
                slen = strlen(ts);
                if (g.client_socket[g.detect[i]] > 0)
                    send(g.client_socket[g.detect[i]], ts, slen, 0);
            }
            else
            {
                sprintf(ts, "Player: %c is not mafia...\n", (char)'A'+choice);
                slen = strlen(ts);
                if (g.client_socket[g.detect[i]] > 0)
                    send(g.client_socket[g.detect[i]], ts, slen, 0);
            }
        }
    }
    log("end of detectChoiceConfirm\n");
}

//=============================================================================
//Doctor Functions
//=============================================================================

void startDoctor()
{
    log("start of startDoctor\n");
    char ts[1640];
    int slen = 0;
    int tempDoctor = g.doctorCount;
    for (int i = 0; i < tempDoctor; i++)
    {
        if (g.player[g.doctor[i]].isDead == true)
            tempDoctor++;
        else
        {
            sprintf(ts, "\nChoose someone to save.\n");
            slen = strlen(ts);
            if (g.client_socket[g.doctor[i]] > 0)
                send(g.client_socket[g.doctor[i]], ts, slen, 0);
        }
    }
    whosAlive(Doctor);
    g.currentAction = Doctor;
    printf("current action: %d\n", g.currentAction);
    log("end of startDoctor\n");
}

void doctorChoiceConfirm(int choice)
{
    log("start of doctorChoiceConfirm\n");
    char ts[1640];
    int slen = 0;
    int tempDoctor = g.doctorCount;
    for (int i = 0; i < tempDoctor; i++)
    {
        if (g.player[g.doctor[i]].isDead == true)
            tempDoctor++;
        else
        {
            sprintf(ts, "You chose player: %c\n", (char)'A'+choice);
            slen = strlen(ts);
            if (g.client_socket[g.doctor[i]] > 0)
                send(g.client_socket[g.doctor[i]], ts, slen, 0);
        }
    }
    log("end of doctorChoiceConfirm\n");
}

//=============================================================================
//Checks
//=============================================================================

void roundCheck()
{
    log("start of roundCheck\n");
    char ts[1640];
    int slen = 0;
    if (g.currentDead == g.currentSave)
    {
        g.currentDead = -1;
        g.currentSave = -1;
        sendAll("The victim was saved by the doctor.\n");
        sendAllAlive("\nCHOOSE SOMEONE TO VOTE OUT\n");
        whosAlive();
    }
    else
    {
        g.currentSave = -1;
        g.player[g.currentDead].isDead = true;
        g.deadCount++;
        if (g.player[g.currentDead].role == Detective)
        {
            g.detectCount--;
        }
        if (g.player[g.currentDead].role == Doctor)
        {
            g.doctorCount--;
        }
        if (g.player[g.currentDead].role == Civilian)
        {
            g.civCount--;
        }
        
        for (int i = 0; i < g.playerCount; i++)
        {
            sprintf(ts, "Player %c has died.\n", (char)'A'+g.currentDead);
            slen = strlen(ts);
            if (g.client_socket[i] > 0)
                send(g.client_socket[i], ts, slen, 0);
        }
        if (checkWin() == false)
        {
            g.currentAction = Civilian;
            printf("current action: %d\n", g.currentAction);
            sendAllAlive("\nCHOOSE SOMEONE TO VOTE OUT\n");
            whosAlive();
        }
    }
    log("end of roundCheck\n");
}

bool voteCheck()
{
    log("start of voteCheck\n");
    char ts[1640];
    int slen = 0;
    g.isTie = false;
    g.votedPlayer = 0;
    //max vote count
    for (int i = 1; i < g.playerCount; i++)
    {
        if (g.player[i].voteCount > g.player[g.votedPlayer].voteCount)
        {
            g.votedPlayer = i;
        }
    }
    //check for tie
    for (int i = 0; i < g.playerCount; i++)
    {
        if (i != g.votedPlayer && 
            g.player[i].voteCount == g.player[g.votedPlayer].voteCount)
        {
            g.isTie = true;
        }
    }
    if (g.isTie == false)
    {
        g.player[g.votedPlayer].isDead = true;
        g.deadCount++;
        if (g.player[g.votedPlayer].role == Mafia)
        {
            g.deadMafia++;
        }
        if (g.player[g.votedPlayer].role == Detective)
        {
            g.detectCount--;
        }
        if (g.player[g.votedPlayer].role == Doctor)
        {
            g.doctorCount--;
        }
        if (g.player[g.votedPlayer].role == Civilian)
        {
            g.civCount--;
        }
        for (int i = 0; i < g.playerCount; i++)
        {
            sprintf(ts, "Player %c has been voted out.\n", (char)'A'+g.votedPlayer);
            slen = strlen(ts);
            if (g.client_socket[i] > 0)
                send(g.client_socket[i], ts, slen, 0);
        }
    }
    log("end of voteCheck\n");
    return g.isTie;
}

bool checkWin()
{
    log("start of checkWin\n");
    if ((g.mafiaCount - g.deadMafia) >= (g.playerCount - g.deadCount - g.mafiaCount + g.deadMafia))
    {
        log("mafia win\n");
        sendAll("Mafia Wins!!\nMafia has successfully killed everyone!\n");
        gameOver();
        log("end of checkWin\n");
        return true;
    }
    if (g.deadMafia >= g.mafiaCount)
    {
        log("civilian win\n");
        sendAll("Civilians Win!!\nMafia has been voted out!\n");
        gameOver();
        log("end of checkWin\n");
        return true;
    }
    log("end of checkWin\n");
    return false;
}

void gameOver()
{
    log("start of gameOver\n");
    sendAll("GAME OVER\n");
    g.currentAction = 0;
    g.isStart = false;
    int tempCount = 0;
    char ts[1640];
    int slen = 0;
    for (int i = 0; i < g.nclients; i++)
    {
        g.player[i].role = Civilian;
        g.player[i].isDead = false;
        g.player[i].hasRole = false;
        g.player[i].hasVoted = false;
        g.player[i].voteCount = 0;
        g.mafiaCount = 1;
        g.deadMafia = 0;
        g.detectCount = 1;
        g.doctorCount = 1;
        g.deadCount = 0;
        g.totalVotes = 0;
        if (g.client_socket[i] > 0)
            tempCount++;
    }
    for (int i =  0; i < g.nclients; i++)
    {
        if (g.client_socket[i] == 0)
        {
            g.client_socket[i] = g.client_socket[i+1];
            g.client_socket[i+1] = 0;
        }
        sprintf(ts, "You are Player: %c\n", (char)'A'+i);
        slen = strlen(ts);
        send(g.client_socket[i], ts, slen, 0);
    }
    g.nclients = tempCount;
    sendAll("\nType 'start' to start another game.\n");
    printf("current action: %d\n", g.currentAction);
    log("end of gameOver\n");
}

void whosAlive()
{
    log("start of whosAlive\n");
    char message[1640];
    int slen = 0;
    sendAllAlive("\nThese are the remaining players:\n");
    for (int i = 0; i < g.playerCount; i++)
    {
        if (g.client_socket[i] > 0 && g.player[i].isDead == false)
        {
            for (int j = 0; j < g.playerCount; j++)
            {
                if (g.player[j].isDead == false && i != j)
                {
                    sprintf(message, "Player: %c\n", (char)'A'+j);
                    slen = strlen(message);
                    send(g.client_socket[i], message, slen, 0);
                }
            }
        }
    }
    log("end of whosAlive\n");
}

void whosAlive(int role)
{
    log("start of whosAlive role\n");
    char message[1640];
    int slen = 0;
    switch(role)
    {
        case 1:
            for (int i = 0; i < g.mafiaCount; i++)
            {
                if (g.client_socket[g.mafia[i]] > 0 && g.player[g.mafia[i]].isDead == false)
                {
                    sprintf(message, "\nThese are the remaining players:\n");
                    slen = strlen(message);
                    send(g.client_socket[g.mafia[i]], message, slen, 0);
                    for (int j = 0; j < g.playerCount; j++)
                    {
                        if (g.player[j].isDead == false && g.player[j].role != Mafia && g.mafia[i] != j)
                        {
                            sprintf(message, "Player: %c\n", (char)'A'+j);
                            slen = strlen(message);
                            send(g.client_socket[g.mafia[i]], message, slen, 0);
                        }
                    }
                }
            }
            break;
        case 2:
            for (int i = 0; i < g.detectCount; i++)
            {
                if (g.client_socket[g.detect[i]] > 0 && g.player[g.detect[i]].isDead == false)
                {
                    sprintf(message, "\nThese are the remaining players:\n");
                    slen = strlen(message);
                    send(g.client_socket[g.detect[i]], message, slen, 0);
                    for (int j = 0; j < g.playerCount; j++)
                    {
                        if (g.player[j].isDead == false && g.player[j].role != Detective && g.detect[i] != j)
                        {
                            sprintf(message, "Player: %c\n", (char)'A'+j);
                            slen = strlen(message);
                            send(g.client_socket[g.detect[i]], message, slen, 0);
                        }
                    }
                }
            }
            break;
        case 3:
            for (int i = 0; i < g.doctorCount; i++)
            {
                if (g.client_socket[g.doctor[i]] > 0 && g.player[g.doctor[i]].isDead == false)
                {
                    sprintf(message, "\nThese are the remaining players:\n");
                    slen = strlen(message);
                    send(g.client_socket[g.doctor[i]], message, slen, 0);
                    for (int j = 0; j < g.playerCount; j++)
                    {
                        if (g.player[j].isDead == false)
                        {
                            sprintf(message, "Player: %c\n", (char)'A'+j);
                            slen = strlen(message);
                            send(g.client_socket[g.doctor[i]], message, slen, 0);
                        }
                    }
                }
            }
            break;
        default:
            break;
    }
    log("end of whosAlive role\n");
}

void playerStatus()
{
    log("start of playerStatus\n");
    for (int i = 0; i < g.playerCount; i++)
    {
        printf("\nPLAYER: %c INFO\n", (char)'A' + i);
        printf("Role: %d\n", g.player[i].role);
        printf("isDead: %d\n", g.player[i].isDead);
        printf("hasVoted: %d\n", g.player[i].hasVoted);
    }
    log("end of playerStatus\n");
}

void log(char *message)
{
    FILE * log;
    log = fopen("log.txt", "a");
    fprintf(log, message);
    fclose(log);
}

void messageFromClient(int playerIdx, char *buffer)
{
    log("start of messageFromClient\n");
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
                g.playerCount = g.nclients;
                sendAll("\nThe game has begun\n");
                assignRoles(g.playerCount);
                g.isStart = true;
                startMafia();
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
                    if (g.client_socket[i] > 0 && i != k)
                        send(g.client_socket[i], ts, slen, 0);
                }
                buff = x + slen;
                done = 1;
            }
        }
        //=====================================================================
        //Mafia Action
        //=====================================================================
        else if (g.currentAction == Mafia)
        {
            bool foundPlayer = false;
            for (int i  = 0; i < g.playerCount; i++)
            {
                if(*buff == (char)'A'+i)
                {
                    g.currentDead = i;
                    foundPlayer = true;
                    break;
                }
                else if(*buff == (char)'a'+i)
                {
                    g.currentDead = i;
                    foundPlayer = true;
                    break;
                }
            }
            if (foundPlayer)
            {
                if (g.player[g.currentDead].isDead == true)
                {
                    sprintf(ts, "Player: %c is dead, pick again\n", (char)'A'+g.currentDead);
                    int slen = strlen(ts);
                    if (g.client_socket[k] > 0)
                        send(g.client_socket[k], ts, slen, 0);
                    done = 1;
                }
                else
                {
                    if (g.detectCount > 0)
                    {
                        g.currentAction++;
                        mafiaChoiceConfirm(g.currentDead);
                        printf("current action: %d\n", g.currentAction);
                        startDetect();
                        done = 1;
                    }
                    else
                    {
                        if (g.doctorCount > 0)
                        {
                            g.currentAction = Doctor;
                            mafiaChoiceConfirm(g.currentDead);
                            printf("current action: %d\n", g.currentAction);
                            startDoctor();
                            done = 1;
                        }
                        else
                        {
                            g.currentAction = Civilian;
                            mafiaChoiceConfirm(g.currentDead);
                            printf("current action: %d\n", g.currentAction);
                            roundCheck();
                            done = 1;
                        }
                    }
                }
            }
            else
            {
                startMafia();
                done = 1;
            }
        }
        //=====================================================================
        //Detective Action
        //=====================================================================
        else if (g.currentAction == Detective)
        {
            bool foundPlayer = false;
            for (int i  = 0; i < g.playerCount; i++)
            {
                if(*buff == (char)'A'+i)
                {
                    g.currentSuspect = i;
                    foundPlayer = true;
                    break;
                }
                else if(*buff == (char)'a'+i)
                {
                    g.currentSuspect = i;
                    foundPlayer = true;
                    break;
                }
            }
            if (foundPlayer)
            {
                if ( g.player[g.currentSuspect].isDead == true)
                {
                    sprintf(ts, "Player: %c is dead, pick again\n", (char)'A'+g.currentSuspect);
                    int slen = strlen(ts);
                    if (g.client_socket[k] > 0)
                        send(g.client_socket[k], ts, slen, 0);
                    done = 1;
                }
                else
                {
                    detectChoiceConfirm(g.currentSuspect);
                    if (g.doctorCount > 0)
                    {
                        g.currentAction++;
                        printf("current action: %d\n", g.currentAction);
                        startDoctor();
                        done = 1;
                    }
                    else
                    {
                        g.currentAction = Civilian;
                        printf("current action: %d\n", g.currentAction);
                        roundCheck();
                        done = 1;
                    }
                }
            }
            else
            {
                startDetect();
                done = 1;
            }
        }
        //=====================================================================
        //Doctor Action
        //=====================================================================
        else if (g.currentAction == Doctor)
        {
            bool foundPlayer = false;
            for (int i  = 0; i < g.playerCount; i++)
            {
                if(*buff == (char)'A'+i)
                {
                    g.currentSave = i;
                    foundPlayer = true;
                    break;
                }
                else if(*buff == (char)'a'+i)
                {
                    g.currentSave = i;
                    foundPlayer = true;
                    break;
                }
            }
            if (foundPlayer)
            {
                if (g.player[g.currentSave].isDead == true)
                {
                    sprintf(ts, "Player: %c is dead, pick again\n", (char)'A'+g.currentSave);
                    int slen = strlen(ts);
                    if (g.client_socket[k] > 0)
                        send(g.client_socket[k], ts, slen, 0);
                    done = 1;
                }
                else
                {
                    doctorChoiceConfirm(g.currentSave);
                    g.currentAction++;
                    printf("current action: %d\n", g.currentAction);
                    roundCheck();
                    done = 1;
                }
            }
            else
            {
                startDoctor();
                done = 1;
            }
        }
        //=====================================================================
        //Voting Round
        //=====================================================================
        else if (g.currentAction == Civilian)
        {
            bool foundPlayer = false;
            int choice = -1;
            for (int i  = 0; i < g.playerCount; i++)
            {
                if(*buff == (char)'A'+i)
                {
                    choice = i;
                    g.player[i].voteCount++;
                    g.totalVotes++;
                    printf("total votes: %d\n", g.totalVotes);
                    g.player[k].hasVoted = true;
                    foundPlayer = true;
                    break;
                }
                else if(*buff == (char)'a'+i)
                {
                    choice = i;
                    g.player[i].voteCount++;
                    g.totalVotes++;
                    printf("total votes: %d\n", g.totalVotes);
                    g.player[k].hasVoted = true;
                    foundPlayer = true;
                    break;
                }
            }
            if (foundPlayer)
            {
                if (g.player[choice].isDead == true)
                {
                    g.player[k].hasVoted = false;
                    g.totalVotes--;
                    sprintf(ts, "Player: %c is dead, pick again\n", (char)'A'+choice);
                    int slen = strlen(ts);
                    if (g.client_socket[k] > 0)
                        send(g.client_socket[k], ts, slen, 0);
                    done = 1;
                }
                else
                {
                    sprintf(ts, "You voted for player: %c\n", (char)'A'+choice);
                    int slen = strlen(ts);
                    if (g.client_socket[k] > 0)
                        send(g.client_socket[k], ts, slen, 0);
                    playerStatus();
                    printf("totalVotes = %d\n", g.totalVotes);
                    printf("playerCount = %d\n", g.playerCount);
                    printf("deadCount = %d\n", g.deadCount);
                    if (g.totalVotes == (g.playerCount - g.deadCount))
                    {
                        printf("INSIDE If total votes: %d\n", g.totalVotes);
                        if (voteCheck() == false)
                        {
                            if (checkWin() == false)
                            {
                                g.totalVotes = 0;
                                for (int i = 0; i < g.playerCount; i++)
                                {
                                    g.player[i].voteCount = 0;
                                    g.player[i].hasVoted = false;
                                }
                                sendAll("NEXT ROUND\n");
                                startMafia();
                                g.currentAction = Mafia;
                                printf("current action: %d\n", g.currentAction);
                            }
                        }
                        else
                        {
                            g.totalVotes = 0;
                            for (int i = 0; i < g.playerCount; i++)
                            {
                                g.player[i].voteCount = 0;
                                g.player[i].hasVoted = false;
                            }
                            sendAllAlive("TIE, VOTE AGAIN\n");
                        }
                    }
                    done = 1;
                }
            }
            else
            {
                sprintf(ts, "player not found, pick again\n");
                int slen = strlen(ts);
                if (g.client_socket[k] > 0)
                    send(g.client_socket[k], ts, slen, 0);
                done = 1;
            }
        }
	}
    log("end of messageFromClient\n");
}

//=============================================================================
//Assigning Player Roles
//=============================================================================

void assignRoles(int playerNum)
{
    log("start of assignRoles\n");
    //assign player roles
    int tempCount = 0;
    char ts[1640];
    char st[1640];
    int slen = 0;
    srand(time(0));
    g.civCount = playerNum - g.mafiaCount - g.detectCount - g.doctorCount;

    //=========================================================================
    //Mafia assign
    //=========================================================================
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
            if (g.client_socket[rnum] > 0)
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
        if (g.client_socket[g.mafia[i]] > 0)
            send(g.client_socket[g.mafia[i]], ts, slen, 0);
    }

    tempCount = 0;
    //=========================================================================
    //Detective assign
    //=========================================================================
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
            if (g.client_socket[rnum] > 0)
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
        if (g.client_socket[g.detect[i]] > 0)
            send(g.client_socket[g.detect[i]], ts, slen, 0);
    }

    tempCount = 0;

    //=========================================================================
    //Doctor assign
    //=========================================================================
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
            if (g.client_socket[rnum] > 0)
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
        if (g.client_socket[g.doctor[i]] > 0)
            send(g.client_socket[g.doctor[i]], ts, slen, 0);
    }

    tempCount = 0;

    //=========================================================================
    //Civilian assign
    //=========================================================================
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
            if (g.client_socket[rnum] > 0)
                send(g.client_socket[rnum], ts, slen, 0);
        }
    }
    log("ROLES ASSIGNED\n");
    log("end of assignRoles\n");
}
