FROM brancc0cdocker/lsfs-base

RUN git clone https://github.com/dsrhaslab/LSFS.git

WORKDIR /LSFS

RUN mkdir build/ && cd build/ && \
	cmake .. && cmake --build . --target peer_exe

EXPOSE 12355
EXPOSE 12356
EXPOSE 12357