CFLAGS = -g -Wall -Wextra -O2 -std=c11

CLIENT_OBJS = 
CLIENT_BIN = siktacka-client
CLIENT_C = client.c

SERVER_OBJS = 
SERVER_BIN = siktacka-server
SERVER_C = server.c

COMMON_OBJS = parser.o err.o rng.o

all: $(CLIENT_BIN) $(SERVER_BIN)

$(CLIENT_BIN): $(CLIENT_OBJS) $(COMMON_OBJS) $(CLIENT_C)
	gcc $(CLIENT_C) $(CLIENT_OBJS) $(COMMON_OBJS) -o $(CLIENT_BIN) $(CFLAGS)

$(SERVER_BIN): $(SERVER_OBJS $(COMMON_OBJS)) $(SERVER_C)
	gcc $(SERVER_C) $(SERVER_OBJS) $(COMMON_OBJS) -o $(SERVER_BIN) $(CFLAGS)

%.o: %.c %.h
	gcc $*.c -c $(CFLAGS)

clean:
	-rm -f *.o
	-rm -f $(CLIENT_BIN) $(SERVER_BIN)
