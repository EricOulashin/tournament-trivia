#!/bin/bash

# makeNixReleaseArchive.sh - Create a Linux or macOS release ZIP archive

OSName=$(uname -s)
version=$(grep DOOR_VERSION source/trivia/doorset.h | sed -E 's/^.*DOOR_VERSION[[:space:]]+"(.*)"/\1/g')
versionWithoutDot=$(echo "$version" | sed 's/\.//g')
releaseDirName="TournamentTrivia_${OSName}"

# Clean up any previous release directory
rm -rf "$releaseDirName"
mkdir -p "$releaseDirName"

# Copy release files, excluding .exe files
for f in release/*; do
    case "$f" in
        *.exe) continue ;;
        *) cp "$f" "$releaseDirName/" ;;
    esac
done

# Copy built binaries from the build directory
cp source/trivia/trivsrv "$releaseDirName/"
cp source/trivia/trivsync "$releaseDirName/"
cp source/trivia/triv32 "$releaseDirName/"
cp source/trivia/trivconfig "$releaseDirName/"
cp source/trivia/regtriv "$releaseDirName/"

# Create the zip file
zipName="TournamentTrivia_${versionWithoutDot}_${OSName}.zip"
rm -f "$zipName"
zip -r -9 "$zipName" "$releaseDirName"

# Clean up
rm -rf "$releaseDirName"

echo "Release archive created: $zipName"
