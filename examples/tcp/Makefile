.PHONY: run all clean

CFLAGS = -I ../../include -O2 -g -Wall -Werror -m64 -ffreestanding -fno-stack-protector -std=gnu99 
DIR = obj

OBJS = obj/main.o 
LIBS = ../../lib/libpacketngin.a
#LIBS = -L ../../lib --start-group -lpacketngin  --end-group

all: $(OBJS)
	ld -melf_x86_64 -nostdlib -e main -o main $^ $(LIBS)

obj/%.o: src/%.c
	mkdir -p $(DIR)
	gcc $(CFLAGS) -c -o $@ $<
	gcc -o server server.c -lpthread
	gcc -o client client.c -lpthread
	gcc -o mul_server multi_server.c -lpthread
	gcc -o integrity integrity.c -lpthread

clean:
	rm -rf obj
	rm -f tcp
	rm -f main
	rm -f server
	rm -f client
	rm -f mul_server
	rm -f integrity

run: all
	../../bin/console script
