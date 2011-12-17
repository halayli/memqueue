src = src/log.c src/http_impl.c src/http_parser.c src/http_str.c \
    src/router.c src/hash.c src/sock_easy.c src/http_conn.c \
    src/memqueue_expiry.c src/memqueue.c common/time.c src/http_bd.c src/main.c \
    src/args_parser.c
ldflags = -Llthread -Lsrc/json/.libs
includes = -I/usr/local/include -I.
gccflags = -g

memqueue : $(src)
	gcc $(gccflags) $(ldflags) -Werror -Wall -lm -ljson -llthread -lpcre $(includes) $(src) -o memqueue

clean:
	rm -f memqueue *.o *.so
