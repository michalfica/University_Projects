traceroute :  traceroute.o
	g++ -std=c++17 -Wall -Wextra -Wshadow -O2 -static -o traceroute \
		 traceroute.o

traceroute.o : traceroute.cpp 
	g++ -std=c++17 -Wall -Wextra -Wshadow -O2 -static -c traceroute.cpp

clean:
	rm traceroute.o

distclean:
	rm traceroute traceroute.o