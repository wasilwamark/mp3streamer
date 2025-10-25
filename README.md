# mp3stream

A lightweight MP3 streaming server that decodes MP3 files and broadcasts raw PCM audio over TCP to multiple clients.

**⚠️ Disclaimer:** This project has only been tested on WSL (Windows Subsystem for Linux) so far. Installation instructions for other platforms are provided but may require adjustments.

## Features

- Streams MP3 audio as raw PCM over TCP (port 5016)
- Supports multiple simultaneous clients
- Loops audio automatically
- Low latency streaming

## Requirements

### Dependencies

- **libmpg123** - MP3 decoding
- **libasound2** (ALSA) - Audio system headers
- **pthread** - Multi-threading support
- **gcc** or compatible C compiler

### Optional (for Ruby client)

- **Ruby** - For the included Ruby client
- **pulseaudio-utils** (paplay) - For audio playback

## Installation

### Linux (Ubuntu/Debian/WSL)

```bash
# Install dependencies
sudo apt update
sudo apt install -y build-essential libmpg123-dev libasound2-dev

# For Ruby client (optional)
sudo apt install -y ruby pulseaudio-utils

# Build the server
gcc mp3stream.c -lmpg123 -lasound -pthread -o mp3stream
```

### Linux (Fedora/RHEL/CentOS)

```bash
# Install dependencies
sudo dnf install -y gcc mpg123-devel alsa-lib-devel

# For Ruby client (optional)
sudo dnf install -y ruby pulseaudio-utils

# Build the server
gcc mp3stream.c -lmpg123 -lasound -pthread -o mp3stream
```

### Linux (Arch/Manjaro)

```bash
# Install dependencies
sudo pacman -S gcc mpg123 alsa-lib

# For Ruby client (optional)
sudo pacman -S ruby

# Build the server
gcc mp3stream.c -lmpg123 -lasound -pthread -o mp3stream
```

### macOS

```bash
# Install dependencies via Homebrew
brew install mpg123

# Build the server (remove -lasound, use CoreAudio instead or omit ALSA headers)
gcc mp3stream.c -lmpg123 -pthread -o mp3stream

# For Ruby client
# Ruby is pre-installed; install pulseaudio if needed:
brew install pulseaudio
```

**Note:** macOS doesn't use ALSA. The current code includes ALSA headers but doesn't use ALSA functions directly, so compilation should work by removing `-lasound`.

### Windows (WSL)

WSL (Windows Subsystem for Linux) is currently the recommended way to run mp3stream on Windows.

```bash
# In WSL (Ubuntu), follow the Linux installation steps
sudo apt update
sudo apt install -y build-essential libmpg123-dev libasound2-dev ruby pulseaudio-utils

# Build
gcc mp3stream.c -lmpg123 -lasound -pthread -o mp3stream
```

**Audio playback in WSL:**
- WSL2 supports PulseAudio for audio playback
- Make sure PulseAudio is configured in your WSL environment
- Alternatively, stream to a Windows client over TCP

## Usage

### Starting the Server

```bash
./mp3stream test.mp3
```

The server will:
- Start listening on TCP port 5016
- Decode the MP3 file to raw PCM
- Loop the audio indefinitely
- Accept multiple client connections

### Connecting Clients

#### Using netcat + aplay (Linux)

```bash
nc localhost 5016 | aplay -f cd -t raw
```

#### Using the Ruby Client

```bash
./pcm_client.rb
```

The Ruby client expects 48 kHz, 16-bit stereo PCM audio.

#### Using netcat only (testing)

```bash
nc localhost 5016 > output.raw
```

Play the raw audio file later:
```bash
aplay -f cd -t raw output.raw
```

### Stopping the Server

Press `Ctrl+C` to gracefully shut down the server.

## Configuration

- **Port:** Default is 5016 (change `PORT` in mp3stream.c)
- **Buffer size:** 4096 bytes (change `PCM_BUF` in mp3stream.c)
- **Audio format:** Determined by the input MP3 file

## Network Streaming

To stream over a network, connect from remote clients:

```bash
nc SERVER_IP 5016 | aplay -f cd -t raw
```

Make sure port 5016 is open in your firewall if streaming across networks.

## License

See source code for license information.