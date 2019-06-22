FROM ubuntu:18.10

RUN apt-get update
RUN apt-get --yes install libncurses5-dev
RUN apt-get --yes install nodejs
RUN apt-get --yes install npm

RUN npm install --global --unsafe-perm=true esy@0.5.8
