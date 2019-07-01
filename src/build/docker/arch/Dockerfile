FROM archlinux/base

RUN pacman --noconfirm -Syu

RUN pacman --noconfirm -S nodejs npm gcc coreutils grep tar sed gawk make diffutils

RUN node --version
RUN npm --version

RUN npm install --global --unsafe-perm=true esy@0.5.8
