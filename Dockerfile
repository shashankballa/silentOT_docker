FROM debian:stable

RUN apt-get update \
    && \
    DEBIAN_FRONTEND=noninteractive apt-get install \
        -y vim git make g++ cmake libtool python

RUN git clone https://github.com/osu-crypto/libOTe.git

RUN cd libOTe \
    && \
    git submodule update --init \
    && \
    mkdir -p out/build/linux \
    && \
    cmake -S . -B out/build/linux \
        -DENABLE_ASAN=ON -DCMAKE_BUILD_TYPE=RelWithDebInfo \
        -DFETCH_AUTO=ON -DENABLE_RELIC=ON -DENABLE_ALL_OT=ON \
        -DENABLE_BOOST=ON -DCOPROTO_ENABLE_BOOST=ON \
        -DENABLE_SILENT_VOLE=ON -DENABLE_BITPOLYMUL=ON \
        -DNO_SILVER_WARNING=ON

RUN cd libOTe \
    && \
    cmake --build out/build/linux 

RUN cd libOTe \
    && \
    su \
    && \
    cmake --install out/build/linux

COPY . SilentOT/

WORKDIR /SilentOT

RUN mkdir build \
    && \
    cd build \
    && \
    cmake .. 

RUN cd build \
    && \
    make