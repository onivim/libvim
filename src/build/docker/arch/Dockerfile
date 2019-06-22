FROM archlinux/base

RUN pacman --noconfirm -Syu

RUN pacman --noconfirm -S nodejs
RUN pacman --noconfirm -S npm
RUN pacman --noconfirm -S gcc
RUN pacman --noconfirm -S coreutils
RUN pacman --noconfirm -S grep
RUN pacman --noconfirm -S tar
RUN pacman --noconfirm -S sed
RUN pacman --noconfirm -S gawk
RUN pacman --noconfirm -S make

RUN node --version
RUN npm --version

RUN npm install --global --unsafe-perm=true esy@0.5.8
