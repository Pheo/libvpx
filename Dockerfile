FROM emscripten/emsdk

# Install dependencies
RUN apt-get update && apt-get install -y yasm

# Set the working directory
WORKDIR /libvpx
