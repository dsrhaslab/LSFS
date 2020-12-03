FROM cpp-build-base:0.1.0 AS build

ARG GITUSER=lsfs-user-temp
ARG GITPASS=Lsfs123User456Temp.,;

RUN git clone https://${GITUSER}:${GITPASS}@github.com/danielsf97/P2P-Filesystem.git

RUN cd P2P-Filesystem && git checkout server-deploy

WORKDIR /P2P-Filesystem

RUN mkdir build/ && cd build/ && \
	cmake .. && cmake --build . --target peer_exe

EXPOSE 12345

EXPOSE 12346

#CMD ["./build/peer_exe", "1", "0.27", "scripts/conf.yaml", "172.17.0.2"]

#FROM ubuntu:bionic

#WORKDIR /opt/lsfs-peer

#COPY --from=build /P2P-Filesystem/build/peer_exe ./
#COPY --from=build /P2P-Filesystem/scripts/conf.yaml ./

#CMD ["./peer_exe","13350","13351","27","0.25","conf.yaml"]
