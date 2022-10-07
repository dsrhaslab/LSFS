FROM brancc0cdocker/lsfs-base

RUN cd .. && wget https://github.com/libfuse/libfuse/releases/download/fuse-3.9.1/fuse-3.9.1.tar.xz && \
	tar -xvf fuse-3.9.1.tar.xz && cd fuse-3.9.1 && \
	apt-get update -y && apt-get install -y udev && \
	mkdir build && cd build && \
	apt install meson -y && meson .. && ninja && ninja install && \ 
	cd ../.. && rm -r fuse-3.9.1.tar.xz

RUN cd .. && apt-get update -y && apt-get install -y automake libtool bison flex && alias yacc="bison" && \
        git clone https://github.com/filebench/filebench.git && \
	cd filebench && libtoolize && aclocal && autoheader && automake --add-missing && \
	autoconf && ./configure && make && make install && \
	cd .. && rm -r filebench

RUN cd .. && cd fuse-3.9.1/build/ && ninja install && cd ../..

RUN git clone https://github.com/dsrhaslab/LSFS.git

WORKDIR /LSFS

RUN mkdir build/ && cd build/ && \
	cmake .. && cmake --build . --target lsfs_exe client_exe

RUN cd / && mkdir test_filesystem/ && mkdir test_filesystem/InnerFolder/ && mkdir test_filesystem/InnerFolder2/

RUN apt install vim -y 

EXPOSE 12365
EXPOSE 12366
EXPOSE 12367