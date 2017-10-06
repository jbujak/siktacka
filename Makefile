CFLAGS = -g -Wall -Wextra -O2 -std=c11
CXXFLAGS = -g -Wall -Wextra -O2 -std=c++14 -lrt -lz

CLIENT_OBJS = map.o gui_client.o
CLIENT_BIN = siktacka-client
CLIENT_C = client.c

SERVER_OBJS = map.o event_queue.o server_msg_queue.o
SERVER_BIN = siktacka-server
SERVER_C = server.c

COMMON_OBJS = parser.o err.o rng.o timer.o

all: $(CLIENT_BIN) $(SERVER_BIN)
	ctags -R .

$(CLIENT_BIN): $(CLIENT_OBJS) $(COMMON_OBJS) $(CLIENT_C)
	gcc -c $(CLFAGS) $(CLIENT_C) -o client.o
	g++ $(CXXFLAGS) client.o $(CLIENT_OBJS) $(COMMON_OBJS) -o $(CLIENT_BIN)

$(SERVER_BIN): $(SERVER_OBJS) $(COMMON_OBJS) $(SERVER_C)
	gcc -c $(CFLAGS) $(SERVER_C) -o server.o
	g++ $(CXXFLAGS) server.o $(SERVER_OBJS) $(COMMON_OBJS) -o $(SERVER_BIN)

%.o: %.c %.h
	gcc $*.c -c $(CFLAGS)

%.o: %.cpp %.h
	g++ $*.cpp -c $(CXXFLAGS) -o $*.o

clean:
	-rm -f *.o
	-rm -f $(CLIENT_BIN) $(SERVER_BIN)
	-rm -f tags
