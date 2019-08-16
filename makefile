#Make file for par-shell

all: par-shell par-shell-terminal

par-shell: par-shell.o list.o pipe-io.o
	gcc -o par-shell -pthread par-shell.o list.o pipe-io.o

par-shell-terminal: par-shell-terminal.o pipe-io.o commandlinereader.o
	gcc -o par-shell-terminal par-shell-terminal.o pipe-io.o commandlinereader.o

par-shell.o: par-shell.c par-shell.h list.h pipe-io.o
	gcc -Wall -g -c par-shell.c

par-shell-terminal.o: par-shell-terminal.c par-shell-terminal.h
	gcc -Wall -g -c par-shell-terminal.c

commandlinereader.o: commandlinereader.h commandlinereader.c 
	gcc -Wall -g -c commandlinereader.c

pipe-io.o: pipe-io.h pipe-io.c
	gcc -Wall -g -c pipe-io.c

list.o: list.c list.h
	gcc -Wall -g -c list.c

clean:
	rm -f *.o par-shell par-shell-terminal *.txt