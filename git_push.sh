#!/bin/bash

# Stop script if any command fails
set -e

# 1) Copy files
cp -r ~/anu/notes/🗺️Toy\ Malloc\ Journey/* ./notes/

# 2) Add changes
git add .

# 3) Read commit message
echo "Enter commit message:"
read message

# 4) Commit
git commit -m "$message"

# 5) Push
git push
