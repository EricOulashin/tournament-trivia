#!/bin/bash

# makeNixReleaseArchive.sh - Create a Linux or macOS release ZIP archive

OSName=$(uname -s)
version=$(grep DOOR_VERSION source/trivia/doorset.h | sed -E 's/^.*DOOR_VERSION[[:space:]]+"(.*)"/\1/g' | tr -d '\r')
versionWithoutDot=$(echo "$version" | sed 's/\.//g')
releaseDirName="TournamentTrivia_${OSName}"

# Clean up any previous release directory
rm -rf "$releaseDirName"
mkdir -p "$releaseDirName"

# Copy release files, excluding Windows binaries (.exe and .dll) and FILE_ID.DIZ
for f in release/*; do
    case "$f" in
        *.exe|*.dll) continue ;;
        */FILE_ID.DIZ) continue ;;
        *) cp "$f" "$releaseDirName/" ;;
    esac
done

# Copy built binaries from the build directory
cp source/trivia/trivsrv "$releaseDirName/"
cp source/trivia/trivsync "$releaseDirName/"
cp source/trivia/triv32 "$releaseDirName/"
cp source/trivia/trivconfig "$releaseDirName/"
cp source/trivia/regtriv "$releaseDirName/"

# Generate FILE_ID.DIZ from template with version, date, and centered OS line
osText="${OSName} Version"
lineWidth=40
padLen=$(( (lineWidth - ${#osText}) / 2 ))
centeredOsLine="$(printf '%*s' "$padLen" '')${osText}"
sed "s/<VERSION>/$version/g" FILE_ID_Template.DIZ \
    | sed "s/<DATE>/$(date '+%Y-%m-%d')/g" \
    | sed "s/<OS_LINE>/$centeredOsLine/g" > FILE_ID.DIZ

# Create the zip file (unless --no-zip is passed, e.g. for CI artifact uploads)
if [ "$1" = "--no-zip" ]; then
    echo "Release directory prepared: $releaseDirName"
else
    zipName="TournamentTrivia_${versionWithoutDot}_${OSName}.zip"
    rm -f "$zipName"
    zip -r -9 "$zipName" FILE_ID.DIZ "$releaseDirName"
    rm -rf "$releaseDirName"
    rm FILE_ID.DIZ
    echo "Release archive created: $zipName"
fi
