#!/bin/bash

for package in *.deb; do dpkg-deb -x "$package" ../i386/; done;
