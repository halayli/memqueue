src = src/log.c src/http_impl.c src/http_parser.c src/http_str.c \
    src/router.c src/hash.c src/sock_easy.c src/http_conn.c \
    src/memqueue_expiry.c src/memqueue.c src/time.c src/http_bd.c src/main.c \
    src/args_parser.c
gccflags = -g

memqueue : $(src)
	gcc -L/usr/local/lib -I/usr/local/include $(gccflags) -Wall -lm -ljson -llthread -lpcre $(src) -Werror -llthread -lpthread -o memqueue

install: memqueue
	cp memqueue /usr/local/bin/memqueue

clean:
	rm -f memqueue *.o *.so
