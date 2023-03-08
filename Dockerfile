# Dockerfile to create devenv for playing around w perf
FROM ubuntu:20.04
ARG DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y \
    curl \
    unzip \
    make \
    build-essential \
    cmake \
    less \ 
    vim \
    linux-tools-generic \
    gdb

 

