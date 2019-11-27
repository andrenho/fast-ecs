#!/bin/sh

gcc -std=c11 -o example example.c fastecs.c -g -O0 -Wall -Wextra && ./example
