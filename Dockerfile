FROM ubuntu:22.04

RUN apt-get update \
    && \
    apt-get install -y \
        vim git make g++ cmake libtool python3 \
        libboost-all-dev libssl-dev libgmp-dev \
        gdb

# Copy, build and install libOTe -----------------------------------------------

WORKDIR /SilentOT/libOTe/

COPY libOTe ./

RUN python3 build.py --all --boost --sodium

RUN python3 build.py --install

# Copy and build silentOT_docker ----------------------------------------------

WORKDIR /SilentOT/

COPY include ./include/

COPY source ./source/

COPY CMakeLists.txt ./

RUN mkdir build \
    && \
    cd build \
    && \
    cmake .. 

RUN cd build \
    && \
    make