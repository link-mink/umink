#!/bin/bash
# mink
echo "Running autoreconf on umINK..."
autoreconf --install --force -I m4 > /dev/null 2>&1 || { echo "umink autoreconf error!"; exit 1; }
