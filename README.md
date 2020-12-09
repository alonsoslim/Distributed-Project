Follow these steps to compile and run the project

First make sure that the Makefile, mafia.cpp, run5.sh and run10.sh are all in the same location

To compile the code:
    Type 'make'

To run the code there are two ways:
    Easy way:
    ./run5.sh //This will run the server file and telnet 5 clients
    OR
    ./run10.sh //This will run the server file and telnet 10 clients

    Hard way:
    ./mafia
    Repeat next line on different terminal sessions to add new clients to the game
    telnet localhost 8888

The run#.sh files use gnome-terminal
Depending on your linux configuration you may have to use mate-terminal.
This could also call for different syntax with the remaining arguments.