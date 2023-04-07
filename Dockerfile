FROM debian:stable

RUN apt-get update \
    && \
    DEBIAN_FRONTEND=noninteractive apt-get install \
        -y vim git make g++ cmake libtool python


# Copy, build and install libOTe -----------------------------------------------

WORKDIR /SilentOT/libOTe/

COPY libOTe ./

RUN mkdir -p out/build/linux \
    && \
    cmake -S . -B out/build/linux \
        -DENABLE_ASAN=ON -DCMAKE_BUILD_TYPE=RelWithDebInfo -DFETCH_AUTO=ON \
        -DENABLE_BOOST=ON -DCOPROTO_ENABLE_BOOST=ON -DENABLE_RELIC=ON \
        -DENABLE_ALL_OT=ON -DENABLE_BITPOLYMUL=ON -DNO_SILVER_WARNING=ON

RUN cmake --build out/build/linux 

RUN su \
    && \
    cmake --install out/build/linux

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