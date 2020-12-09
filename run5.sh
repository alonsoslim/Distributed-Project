#! /bin/sh

echo "running mafia multiplayer game..."

gnome-terminal --geometry=80x28 -- ./mafia
gnome-terminal --geometry=40x15 -- telnet localhost 8888
gnome-terminal --geometry=40x15 -- telnet localhost 8888
gnome-terminal --geometry=40x15 -- telnet localhost 8888
gnome-terminal --geometry=40x15 -- telnet localhost 8888
gnome-terminal --geometry=40x15 -- telnet localhost 8888

echo "done."
