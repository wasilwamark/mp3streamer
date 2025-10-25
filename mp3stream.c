/*
 *  mp3stream.c  – tiny MP3 streaming server
 *  build:  gcc mp3stream.c -lmpg123 -lasound -o mp3stream
 *  usage:  ./mp3stream  file.mp3
 *  listen: nc $SERVER_IP 5016 | aplay -f cd -t raw
 */
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <alsa/asoundlib.h>
#include <mpg123.h>

#define PORT      5016
#define PCM_BUF   4096

static volatile int keep_running = 1;
static pthread_mutex_t client_mutex = PTHREAD_MUTEX_INITIALIZER;
static int server_fd = -1;

typedef struct client {
    int fd;
    struct client *next;
} client_t;
static client_t *clients = NULL;

/* add a newly-accepted client to the linked list */
static void client_add(int fd)
{
    client_t *c = malloc(sizeof(*c));
    c->fd = fd;
    pthread_mutex_lock(&client_mutex);
    c->next = clients;
    clients = c;
    pthread_mutex_unlock(&client_mutex);
}
 
/* remove closed client */
static void client_remove(int fd)
{
    client_t **p = &clients, *c;
    pthread_mutex_lock(&client_mutex);
    while ((c = *p)) {
        if (c->fd == fd) {
            *p = c->next;
            free(c);
            break;
        }
        p = &c->next;
    }
    pthread_mutex_unlock(&client_mutex);
}

/* broadcast PCM buffer to every connected client */
static void broadcast(const unsigned char *buf, size_t len)
{
    client_t **p = &clients, *c;
    pthread_mutex_lock(&client_mutex);
    while ((c = *p)) {
        if (write(c->fd, buf, len) != (ssize_t)len) {
            /* client dead */
            close(c->fd);
            *p = c->next;
            free(c);
        } else {
            p = &c->next;
        }
    }
    pthread_mutex_unlock(&client_mutex);
}

/* TCP listener thread */
static void *listener_thread(void *arg)
{
    struct sockaddr_in addr;
    socklen_t alen = sizeof(addr);
    while (keep_running) {
        int fd = accept(server_fd, (struct sockaddr *)&addr, &alen);
        if (fd < 0) { perror("accept"); break; }
        fprintf(stderr, "[+] client %s:%d connected\n",
                inet_ntoa(addr.sin_addr), ntohs(addr.sin_port));
        client_add(fd);
    }
    return NULL;
}

/* graceful shutdown */
static void sighandler(int sig)
{
    (void)sig;
    keep_running = 0;
}

int main(int argc, char **argv)
{
    if (argc != 2) {
        fprintf(stderr, "usage: %s file.mp3\n", argv[0]);
        return 1;
    }

    /* ---- init MP3 decoder ---- */
    mpg123_init();
    mpg123_handle *mh = mpg123_new(NULL, NULL);
    if (!mh)  { fprintf(stderr, "mpg123_new failed\n"); return 2; }
    if (mpg123_open(mh, argv[1]) != MPG123_OK) {
        fprintf(stderr, "cannot open %s\n", argv[1]); return 2; }
    long rate;
    int  channels, enc;
    mpg123_getformat(mh, &rate, &channels, &enc);
    fprintf(stderr, "MP3 info: %ld Hz %d ch enc:%d\n", rate, channels, enc);

    /* ---- open TCP server ---- */
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    struct sockaddr_in serv_addr = {
        .sin_family = AF_INET,
        .sin_port   = htons(PORT),
        .sin_addr   = { INADDR_ANY }
    };
    if (bind(server_fd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0 ||
        listen(server_fd, 5) < 0) {
        perror("bind/listen"); return 3;
    }
    fprintf(stderr, "TCP server on port %d – waiting for clients…\n", PORT);

    pthread_t tid;
    pthread_create(&tid, NULL, listener_thread, NULL);

    signal(SIGINT, sighandler);
    signal(SIGTERM, sighandler);

    /* ---- decode & broadcast loop ---- */
    size_t done;
    unsigned char pcm[PCM_BUF];
    while (keep_running) {
        int err = mpg123_read(mh, pcm, PCM_BUF, &done);
        if (err == MPG123_DONE) {
            mpg123_seek(mh, 0, SEEK_SET);          /* endless loop */
            continue;
        }
        if (err != MPG123_OK) { fprintf(stderr, "mpg123_read: %s\n",
                                        mpg123_plain_strerror(err)); break; }
        broadcast(pcm, done);
    }

    /* ---- cleanup ---- */
    keep_running = 0;
    close(server_fd);
    pthread_join(tid, NULL);
    mpg123_close(mh);
    mpg123_delete(mh);
    mpg123_exit();
    return 0;
}