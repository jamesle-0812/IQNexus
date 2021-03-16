#!/bin/bash

printf "Adding Post-commit version update\r\n"
cp metaScripts/version.sh .git/hooks/post-commit

printf "Adding Post-checkout version update\r\n"
cp metaScripts/version.sh .git/hooks/post-checkout

printf "Setting execute permissions\r\n"
chmod +x .git/hooks/post-commit
chmod +x .git/hooks/post-checkout

printf "running version.sh"
metaScripts/version.sh

