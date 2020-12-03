FROM cpp-build-base:0.1.0 AS build

ARG GITUSER=lsfs-user-temp
ARG GITPASS=Lsfs123User456Temp.,;

RUN git clone https://${GITUSER}:${GITPASS}@github.com/danielsf97/P2P-Filesystem.git

RUN apt-get update -y && apt-get install -y automake libtool bison flex && alias yacc="bison" && \
        git clone https://github.com/filebench/filebench.git && \
	cd filebench && libtoolize && aclocal && autoheader && automake --add-missing && \
	autoconf && ./configure && make && make install && \
	cd .. && rm -r filebench

RUN cd fuse-3.9.1/build/ && ninja install && cd ../..

RUN cd P2P-Filesystem && git checkout server-deploy

WORKDIR /P2P-Filesystem

RUN mkdir build/ && cd build/ && \
	cmake .. && cmake --build . --target lsfs_exe

RUN mkdir Teste/ && mkdir Teste/InnerFolder/ && mkdir Teste/InnerFolder2/

EXPOSE 12345

EXPOSE 12346

