```bash
#!/usr/bin/env bash
# mp3stream-lab.sh – one-shot installer + runner
# 1. installs C & Ruby deps
# 2. builds C server
# 3. grabs a free MP3
# 4. starts server + Ruby client in two tmux panes (or plain terminals)

set -e
cd "$(dirname "$0")"

echo "=== 1.  Install dependencies ==="
sudo apt update -qq
sudo apt install -y build-essential libmpg123-dev libasound2-dev ruby pulseaudio-utils

echo "=== 2.  Build C server ==="
cat > mp3stream.c <<'C_EOF'
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
#define PORT 5016
#define PCM_BUF 4096
static volatile int keep_running=1;
static pthread_mutex_t mx=PTHREAD_MUTEX_INITIALIZER;
static int srv=-1;
typedef struct client{int fd;struct client*next;}client_t;
static client_t*clients=NULL;
static void client_add(int fd){
    client_t*c=malloc(sizeof*c);c->fd=fd;
    pthread_mutex_lock(&mx);c->next=clients;clients=c;pthread_mutex_unlock(&mx);
}
static void broadcast(const unsigned char*buf,size_t len){
    client_t**p=&clients,*c;
    pthread_mutex_lock(&mx);
    while((c=*p)){
        if(write(c->fd,buf,len)!=(ssize_t)len){close(c->fd);*p=c->next;free(c);}
        else p=&c->next;
    }
    pthread_mutex_unlock(&mx);
}
static void*listener(void*arg){
    struct sockaddr_in a;socklen_t l=sizeof(a);
    while(keep_running){int f=accept(srv,(struct sockaddr*)&a,&l);if(f<0)break;client_add(f);}
    return NULL;
}
static void sigh(int s){(void)s;keep_running=0;}
int main(int c,char**v){
    if(c!=2){fprintf(stderr,"usage: %s file.mp3\n",v[0]);return 1;}
    mpg123_init();mpg123_handle*m=mpg123_new(NULL,NULL);
    if(!m||mpg123_open(m,v[1])!=MPG123_OK)return 2;
    long r;int ch,enc;mpg123_getformat(m,&r,&ch,&enc);
    printf("MP3 info: %ld Hz %d ch enc:%d\n",r,ch,enc);

    srv=socket(AF_INET,SOCK_STREAM,0);int opt=1;
    setsockopt(srv,SOL_SOCKET,SO_REUSEADDR,&opt,sizeof opt);
    struct sockaddr_in s={.sin_family=AF_INET,.sin_port=htons(PORT),.sin_addr={INADDR_ANY}};
    if(bind(srv,(struct sockaddr*)&s,sizeof s)<0||listen(srv,5)<0){perror("bind/listen");return 3;}
    printf("TCP server on port %d – waiting for clients…\n",PORT);

    pthread_t t;pthread_create(&t,NULL,listener,NULL);
    signal(SIGINT,sigh);signal(SIGTERM,sigh);

    size_t done;unsigned char pcm[PCM_BUF];
    while(keep_running){
        int e=mpg123_read(m,pcm,PCM_BUF,&done);
        if(e==MPG123_DONE){mpg123_seek(m,0,SEEK_SET);continue;}
        if(e!=MPG123_OK){fprintf(stderr,"mpg123_read: %s\n",mpg123_plain_strerror(e));break;}
        broadcast(pcm,done);
    }
    keep_running=0;close(srv);pthread_join(t,NULL);
    mpg123_close(m);mpg123_delete(m);mpg123_exit();return 0;
}
C_EOF
gcc mp3stream.c -lmpg123 -lasound -pthread -o mp3stream

echo "=== 3.  Grab a free MP3 ==="
curl -L -s https://www.soundjay.com/misc/sounds/bell-ringing-05.mp3 > test.mp3

echo "=== 4.  Ruby client (zero gems) ==="
cat > pcm_client.rb <<'R_EOF'
#!/usr/bin/env ruby
require 'socket'
class PCMClient
  HOST='localhost'; PORT=5016; CHUNK=4096
  def initialize
    @sock=TCPSocket.new(HOST,PORT)
    @player=IO.popen('paplay --raw --format=s16le --channels=2 --rate=48000','w')
  end
  def stream
    puts '[*] streaming 48 kHz / 16-bit / stereo …  Ctrl-C to stop'
    loop{@player.write(@sock.read(CHUNK))}
  rescue Interrupt; puts"\n[*] shutting down"
  ensure; close; end
  private; def close; @sock.close; @player.close; end
end
PCMClient.new.stream if __FILE__==$0
R_EOF
chmod +x pcm_client.rb

echo "=== 5.  Run ==="
echo "Starting server in background …"
./mp3stream test.mp3 &
sleep 2
echo "Starting Ruby client …"
./pcm_client.rb
```