#SenDeca
This is the source repo for the SenDeca project, a line of sensum IoT devices based on the CMWX1ZZABZ-901 LoRa chip.
The SenDeca will be capible of communicating with I2C, UART and MODBUS devices, along with generic digital and analog IO.

After checking out the repo, you should run ./init\_githooks.sh, to set up the hooks that insert version information into the program.

If you checkout a commit which existed before the version printing feature was implemented, then it is advised that you remove the git hooks by running "rm .git/hooks/post\_commit" and "rm .git/hooks/post\_checkout"

The git hooks currently only work in a Bash shell, as they are bash scripts
#More documentation to come
