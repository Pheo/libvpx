FROM emscripten/emsdk:3.1.40

# Install dependencies
RUN apt-get update && apt-get install -y yasm

# Set the working directory
WORKDIR /libvpx
