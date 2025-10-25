#!/usr/bin/env ruby
# pcm_client.rb

require 'socket'

class PCMClient
  HOST  = 'localhost'
  PORT  = 5016
  CHUNK = 4096          # read buffer

  def initialize
    @sock = TCPSocket.new(HOST, PORT)
    # Use PulseAudio (WSLg-friendly) instead of ALSA aplay
    @player = IO.popen('paplay --format=s16le --channels=2 --rate=48000 --raw', 'w')
  end

  def stream
    puts '[*] streaming 48 kHz / 16-bit / stereo …  Ctrl-C to stop'
    loop { @player.write(@sock.read(CHUNK)) }
  rescue Interrupt
    puts "\n[*] shutting down"
  ensure
    close
  end

  private

  def close
    @sock.close
    @player.close
  end
end

# --------------------------
PCMClient.new.stream if __FILE__ == $0