# ADR 0197: Cooperative CLI export interruption

The standalone command installs a SIGINT handler after argument parsing.
The handler performs only a relaxed store to the process-lifetime cancellation
token. A compile-time lock-free check ensures that store does not require a
lock. Normal execution passes the same token to the static OBJ and FBX owners.

The publisher checks the token before staged write and flush attempts and
immediately before rename. If cancellation is observed before rename, ordinary
unwinding removes the staged file. Rename is the commit boundary. A signal
racing with rename may arrive too late to prevent commit, and an already
committed job retains its file and success receipt. A batch stops before the
next job when it observes cancellation. A cancelled job returns 130.

Pure tests inject cancellation before and after the commit boundary without
sending signals. Ordinary Capsule exports exercise the resulting binaries.
Live SIGINT delivery remains a separate acceptance check under Apple #186.
