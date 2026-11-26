# Security

Do not post credentials, personal captures or unredacted device logs in public issues. Until a private security-reporting channel is configured on the published repository, report only a minimal non-sensitive description and ask the maintainer for a private channel.

The installer pins dependency hashes, pins the tablet SSH identity, restricts device/firmware support, and runs Electron with a sandboxed renderer and narrow IPC. Root access and native hooks are still inherently powerful. Backups and an accessible stock recovery path are required. This beta has not had an independent security audit.
