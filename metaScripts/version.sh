#!/bin/bash

VERSION=`git log --pretty=oneline | wc -l`
HASH=`git rev-parse HEAD`
BRANCH=`git name-rev --name-only HEAD`
MESSAGE=`git log --pretty=oneline | head -n1 | sed 's/[^ ]* *//'`

printf "\r\n\r\nversion\t: %s\r\n" "$VERSION"
printf "hash\t: %s\r\n" "$HASH"
printf "branch\t: %s\r\n" "$BRANCH"
printf "message\t: %s\r\n" "$MESSAGE"



#now lets make a file

cat > Project/inc/sensum_version.h <<EOF
#ifndef VERSION_HEADER
#define VERSION_HEADER

#define VERSION_NUMBER  $VERSION
#define VERSION_HASH    "$HASH"
#define VERSION_BRANCH  "$BRANCH"
#define VERSION_MESSAGE "$MESSAGE"

#endif
EOF

