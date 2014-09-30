#!/bin/bash

sudo apt-get install gcc-multilib
for package in *_i386.deb *_all.deb; do dpkg-deb -x "$package" ../i386/; done;
for package in *_amd64.deb *_all.deb; do dpkg-deb -x "$package" ../x86_64/; done;
