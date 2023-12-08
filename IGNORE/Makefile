CC = gcc
CFLAGS = -O3 -Wall -Wextra -Werror -ansi -pedantic
 
all: readdir threads mutex

readdir: readdir.c
	$(CXX) $(CFLAGS) -o readdir readdir.c

threads: threads.c
	$(CXX) $(CFLAGS) -o threads threads.c

mutex: mutex.c
	$(CXX) $(CFLAGS) -o mutex mutex.c

clean:
	rm -f readdir