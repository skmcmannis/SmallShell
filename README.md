# Small Shell

A Unix shell meant to run on Bash. Runs Unix programs. Supports I/O redirect and background processing.

Built-in Commands:

'cd' - Changes working directory to the provided parameter, or to HOME if none is provided.

'status' - Prints either the exit status or terminating signal of the most recent foreground process.

'exit' - Kills all processes and exits.

Supports variable expansion for '$$', expanding to the PID of the current process.

Includes signal handling for SIGINT and SIGTSTP.