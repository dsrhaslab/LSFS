FROM ubuntu:bionic

RUN apt-get update && \
	apt-get install -y build-essential git autoconf libtool pkg-config apt-utils

RUN apt-get install -y wget \
  && rm -rf /var/lib/apt/lists/*

RUN version=3.15 && build=4 && \
	mkdir ~/temp && cd ~/temp && \
	wget https://cmake.org/files/v$version/cmake-$version.$build.tar.gz && \
	tar -xzvf cmake-$version.$build.tar.gz && \
	cd cmake-$version.$build/ && \
	./bootstrap --prefix=/usr/ && \
	make -j $(nproc) && \
	make -j $(nproc) install && \
	cd ../.. && rm -r temp/

#RUN wget https://capnproto.org/capnproto-c++-0.7.0.tar.gz && \
#	tar zxf capnproto-c++-0.7.0.tar.gz && \
#	cd capnproto-c++-0.7.0 && \
#	./configure --prefix=/usr/ && \
#	make -j $(nproc) check && \
#	make -j $(nproc) install && \
#	cd .. && \
#	rm -r capnproto-c++-0.7.0 capnproto-c++-0.7.0.tar.gz

RUN git clone https://github.com/nlohmann/json.git && \
	cd json && mkdir build && cd build && \
	cmake .. && make -j $(nproc) && \
	make -j $(nproc) install && \
	cd ../.. && rm -r json

RUN apt-get update && apt-get install gcc-7 g++-7 gcc-8 g++-8 -y && \
        update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-7 70 --slave /usr/bin/g++ g++ /usr/bin/g++-7 && \
        update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-8 50 --slave /usr/bin/g++ g++ /usr/bin/g++-8 && \
        update-alternatives --set gcc "/usr/bin/gcc-8"


RUN wget https://dl.bintray.com/boostorg/release/1.71.0/source/boost_1_71_0.tar.gz && \
	tar xzvf boost_1_71_0.tar.gz && \
	cd boost_1_71_0 && \
	apt-get update && apt-get install g++ python-dev autotools-dev libicu-dev libbz2-dev libboost-all-dev -y && \
	./bootstrap.sh --prefix=/usr/ && \
	./b2 && ./b2 install && \
	cd .. && rm -r boost_1_71_0 boost_1_71_0.tar.gz

RUN wget https://github.com/protocolbuffers/protobuf/releases/download/v3.11.4/protobuf-cpp-3.11.4.tar.gz && \
	tar zxf protobuf-cpp-3.11.4.tar.gz && \
	cd protobuf-3.11.4 && \
	./configure --prefix=/usr/ && \
	make -j $(nproc) && make -j $(nproc) check && \
	make -j $(nproc) install && ldconfig && \
	cd .. && rm -r protobuf-3.11.4 protobuf-cpp-3.11.4.tar.gz

RUN git clone https://github.com/jbeder/yaml-cpp.git && \
	cd yaml-cpp && \
	mkdir build/ && cd build/ && \
	cmake .. && make -j $(nproc) . && make -j $(nproc) install && \
	cd ../.. && rm -r yaml-cpp

RUN git clone https://github.com/gabime/spdlog.git && \
	cd spdlog && mkdir build && cd build && \
	cmake .. && make -j $(nproc) && make -j $(nproc) install && \
	cd ../.. && rm -r spdlog

RUN git clone --recurse-submodules https://github.com/google/leveldb.git && \
	cd leveldb && mkdir build && cd build && \
	cmake -DCMAKE_BUILD_TYPE=Release .. && cmake --build . && \
	make -j $(nproc) . && make -j $(nproc) install && \
	cd ../.. && rm -r leveldb

RUN git clone https://github.com/wiredtiger/wiredtiger.git && \
	cd wiredtiger && bash autogen.sh && ./configure --prefix=/usr && \
	make -j $(nproc) && make -j $(nproc) install && \
	cd .. && rm -r wiredtiger

RUN wget https://github.com/libfuse/libfuse/releases/download/fuse-3.9.1/fuse-3.9.1.tar.xz && \
	tar -xvf fuse-3.9.1.tar.xz && cd fuse-3.9.1 && \
	apt-get update -y && apt-get install -y udev && \
	mkdir build && cd build && \
	apt install meson -y && meson .. && ninja && ninja install
