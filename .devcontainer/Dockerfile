FROM ubuntu:latest

ENV DEBIAN_FRONTEND noninteractive

RUN apt-get update
RUN apt-get install -y cmake clang libgtest-dev
RUN rm -rf /var/lib/apt/lists/* 

WORKDIR "/mnt"
