#!/bin/sh
# Run a GTK test program (TAP on stdout) under xvfb-run.
#
# xvfb-run sends the program's stderr to stdout, where GTK's log messages
# would corrupt the TAP stream, so keep the original stderr on fd 3 and
# give it back to the program.
exec 3>&2
exec xvfb-run -a -s "-screen 0 1024x768x24" sh -c 'exec "$@" 2>&3' sh "$@"
