all: mafia

mafia: mafia.cpp
	g++ mafia.cpp -Wall -o mafia

clean:
	rm -f mafia
	rm log.txt