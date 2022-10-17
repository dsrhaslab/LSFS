FROM ubuntu:focal

ARG DEBIAN_FRONTEND=noninteractive

RUN apt-get update && \
	apt-get install -y build-essential git autoconf libtool pkg-config apt-utils

RUN apt-get install -y wget \
  && rm -rf /var/lib/apt/lists/*

RUN wget https://github.com/libfuse/libfuse/releases/download/fuse-3.9.1/fuse-3.9.1.tar.xz && \
	tar -xvf fuse-3.9.1.tar.xz && cd fuse-3.9.1 && \
	apt-get update -y && apt-get install -y udev && \
	mkdir build && cd build && \
	apt install meson -y && meson .. && ninja && ninja install && \ 
	cd ../.. && rm -r fuse-3.9.1.tar.xz

RUN apt-get update -y && apt-get install -y automake libtool bison flex && alias yacc="bison" && \
        git clone https://github.com/filebench/filebench.git && \
	cd filebench && libtoolize && aclocal && autoheader && automake --add-missing && \
	autoconf && ./configure && make && make install && \
	cd .. && rm -r filebench

RUN cd fuse-3.9.1/build/ && ninja install && cd ../..

RUN git clone https://github.com/libfuse/libfuse.git

RUN cd / && mkdir test_filesystem/ && mkdir test_filesystem/InnerFolder/ && mkdir test_filesystem/InnerFolder2/

RUN sed -i '74s/.*/	cfg->direct_io = 1;/' fuse-3.9.1/example/passthrough.c

RUN gcc -Wall fuse-3.9.1/example/passthrough.c `pkg-config fuse3 --cflags --libs` -o passthrough