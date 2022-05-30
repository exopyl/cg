FROM gcc:12.1.0

ENV DEBIAN_FRONTEND noninteractive

RUN apt-get update
RUN apt-get install -y cmake libgtest-dev libboost-test-dev
RUN rm -rf /var/lib/apt/lists/* 

WORKDIR "/mnt"
