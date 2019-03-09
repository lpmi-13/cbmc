FROM ubuntu:16.04

RUN apt-get update && apt-get install g++-5 flex bison \
    make libwww-perl patch wget cmake libubsan0 parallel \
    jq build-essential -y

RUN mkdir ~/bin
WORKDIR ~/bin

RUN wget https://github.com/diffblue/cbmc/archive/cbmc-5.10.tar.gz

RUN tar -xvf cbmc-5.8.tar.gz

WORKDIR cbmc-cbmc-5.8/src 

RUN make minisat2-download && make
