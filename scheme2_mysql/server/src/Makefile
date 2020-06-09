SOURCES:=$(wildcard *.c)
OBJS:=$(patsubst %.c,%.o,$(SOURCES))
ELF:=thread_server
CC:=gcc
CFLAGS:=-Wall -I../include
$(ELF):$(OBJS)
	gcc $^ -o $@ -lpthread -lmysqlclient
clean:
	rm -rf $(OBJS) $(ELF)

