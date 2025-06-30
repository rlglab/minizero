# This docker file is to build a zero training environment
FROM nvidia/cuda:11.6.2-cudnn8-devel-ubuntu20.04

ARG DEBIAN_FRONTEND=noninteractive

ENV TZ=Asia/Taipei

RUN apt-get update && \
    apt-get install -y --no-install-recommends \
    vim \
    gdb \
    git \
    less \
    htop \
    wget \
    cmake \
    ffmpeg \
    clang-format \
    python3-pip \
    python3-dev \
    python3-opencv \
    python3-pybind11 \
    libopencv-dev \
    libarchive-tools \
    libboost-all-dev && \
    ln -s /usr/bin/python3 /usr/bin/python && \
    python3 -m pip install -U pip \
    gdown \
    cmake \
    numpy \
    pandas \
    jupyter \
    matplotlib && \
    python3 -m pip cache purge && \
    apt-get clean && rm -rf /var/lib/apt/lists/*

# install pytorch & libtorch
RUN python3 -m pip install torch==1.13.0+cu116 --extra-index-url https://download.pytorch.org/whl/cu116 && \
    wget -qO- https://download.pytorch.org/libtorch/cu116/libtorch-cxx11-abi-shared-with-deps-1.13.0%2Bcu116.zip | bsdtar -xvf- -C /usr/local && \
    rm /usr/local/libtorch/lib/libcudnn.so.8
ENV CMAKE_PREFIX_PATH /usr/local/libtorch/share/cmake/Torch

# install gym/atari & Arcade Learning Environment (ALE v0.8.0)
# You can download Atari game ROMs from the following link: https://www.atarimania.com/rom_collection_archive_atari_2600_roms.html
COPY atari57/ /opt/atari57
RUN python3 -m pip install gym[atari] && \
    python3 -m pip cache purge && \
    ale-import-roms /opt/atari57 && \
    cd /opt/ && \
    git clone https://github.com/mgbellemare/Arcade-Learning-Environment.git && \
    cd Arcade-Learning-Environment && \
    git checkout d59d00688b58c5c14dff5fc79db5c22e86987f5d && \
    mkdir build && cd build && \
    cmake ../ -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_STANDARD=17 -DBUILD_PYTHON_LIB=OFF && \
    cmake --build . --target install -j && \
    cd /opt/ && rm -rf Arcade-Learning-Environment

# install gogui
RUN apt-get update && \
    apt-get install -y unzip gconf2 default-jre && \
    gdown https://drive.google.com/uc?id=1wF65yyD5BjL0MkZRFCWLvmqWYx8TDgk0 && \
    unzip gogui-1.4.9.zip && \
    echo "exit 0" >> gogui-1.4.9/install.sh && \
    cd gogui-1.4.9 && \
    ./install.sh && \
    cd .. && \
    rm -rf gogui-1.4.9 gogui-1.4.9.zip && \
    apt-get clean && rm -rf /var/lib/apt/lists/*

RUN mkdir /workspace
WORKDIR /workspace
