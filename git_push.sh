#!/bin/bash

# Stop script if any command fails
set -e

# 1) Copy files
cp -r ~/anu/notes/🗺️Toy\ Malloc\ Journey/* ./notes/
echo "Copied files successfully ✅"

# 2) Add changes
git add .
echo "Changes staged for commit"

# 3) Read commit message
echo "Enter commit message:"
read message

# 4) Commit
git commit -m "$message"

# 5) Push
git push
echo "Files pushed successfully ✅"
