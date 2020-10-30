#include <stdio.h>
#include <stdlib.h>
#include <time.h>

const int MAX_PLAYERS = 10;

enum role {
    Mafia = 1,
    Detective,
    Doctor,
    Civilian
};

class Player {
    public:
    int number;
    char name[30];
    role role;
    bool hasRole = false;
    bool isDead = false;
    bool hasVoted = false;
};

class Global {
    public:
    int mafiaCount = 2;
    int detectCount = 1;
    int doctorCount = 1;
    int civCount = MAX_PLAYERS - mafiaCount
                - detectCount- doctorCount;
    Player player[MAX_PLAYERS];
    Global() {}
} g;

void assignRoles(int playerNum);

/*
void trialRun();
void discussionLoop();
void mainLoop();
bool checkWin();
*/

int main () {
    bool done = false;
    assignRoles(10);
    for (int i = 0; i < 10; i++) {
        printf("Player: %d\n", i);
        printf("Role: %d\n", g.player[i].role);
    }
    //trialRun();
    //while(!done) 
    //    mainLoop();
    //    discussionLoop();
    //    if (checkWin() == true)
    //        done = true;
    return 0;
}

void assignRoles(int playerNum) {
    //assign player roles
    //Mafia assign
    srand(time(0));
    while (g.mafiaCount > 0) {
        int rnum = rand() % 10;
        if (g.player[rnum].hasRole == false) {
            g.player[rnum].role = Mafia;
            g.player[rnum].hasRole = true;
            g.mafiaCount--;
        }
    }
    //Detective assign
    while (g.detectCount > 0) {
        int rnum = rand() % 10;
        if (g.player[rnum].hasRole == false) {
            g.player[rnum].role = Detective;
            g.player[rnum].hasRole = true;
            g.detectCount--;
        }
    }
    //Doctor assign
    while (g.doctorCount > 0) {
        int rnum = rand() % 10;
        if (g.player[rnum].hasRole == false) {
            g.player[rnum].role = Doctor;
            g.player[rnum].hasRole = true;
            g.doctorCount--;
        }
    }
    //Civilian assign
    while (g.civCount > 0) {
        int rnum = rand() % 10;
        if (g.player[rnum].hasRole == false) {
            g.player[rnum].role = Civilian;
            g.player[rnum].hasRole = true;
            g.civCount--;
        }
    }
}
/*
void trialRun() {
    //everyone down

    //mafia up
    //mafia down

    //detective up
    //detective down

    //doctor up
    //doctor down

    //everyone up
}

void mainLoop() {
    // everyone down

    //mafia up
    //mafia kill
    //mafia down

    //detective up
    //detective sus
    //detective down

    //doctor up
    //doctor save
    //doctore down

    //everyone up
}

void discussionLoop() {
    //discussion and voting
}

bool checkWin() {
    //
}
*/