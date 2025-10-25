#!/usr/bin/env python3
import socket
import subprocess
import sys
import signal


class PCMClient:
    HOST = 'localhost'
    PORT = 5016
    CHUNK = 4096

    def __init__(self):
        self.sock = None
        self.player = None

    def connect(self):
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.sock.connect((self.HOST, self.PORT))
        
        self.player = subprocess.Popen(
            ['paplay', '--raw', '--format=s16le', '--channels=2', '--rate=48000'],
            stdin=subprocess.PIPE
        )

    def stream(self):
        print('[*] streaming 48 kHz / 16-bit / stereo … Ctrl-C to stop')
        try:
            while True:
                data = self.sock.recv(self.CHUNK)
                if not data:
                    break
                self.player.stdin.write(data)
                self.player.stdin.flush()
        except KeyboardInterrupt:
            print('\n[*] shutting down')
        finally:
            self.close()

    def close(self):
        if self.sock:
            self.sock.close()
        if self.player:
            self.player.stdin.close()
            self.player.terminate()
            self.player.wait()


if __name__ == '__main__':
    client = PCMClient()
    client.connect()
    client.stream()
