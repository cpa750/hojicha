#!/bin/sh
if echo "$1" | grep -Eq 'i[[:digit:]]86-'; then
  echo i386
#TODO take elf out of x86_64-elf
else
  echo "$1" | grep -Eo '^[[:alnum:]_]*'
fi

