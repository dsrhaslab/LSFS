FROM ubuntu:bionic

ARG GITUSER=lsfs-user-temp
ARG GITPASS=Lsfs123User456Temp.,;

RUN apt-get update && \
	apt-get install -y build-essential git autoconf libtool pkg-config apt-utils

RUN apt-get update -y && apt-get install -y automake libtool bison flex && alias yacc="bison" && \
        git clone https://github.com/filebench/filebench.git && \
	cd filebench && libtoolize && aclocal && autoheader && automake --add-missing && \
	autoconf && ./configure && make && make install && \
	cd .. && rm -r filebench

RUN git clone https://${GITUSER}:${GITPASS}@github.com/danielsf97/P2P-Filesystem.git

RUN cd P2P-Filesystem && git checkout fuse-integration && \
	mkdir Teste && mkdir Teste/InnerFolder && mkdir Teste/InnerFolder2