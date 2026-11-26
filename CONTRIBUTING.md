# Contributing

Read docs/BUILD.md, run the desktop tests, and add focused tests for behavior changes. Keep secrets and user data outside the repository. Changes to native ink hooks require testing on the exact firmware/device, including cancellation, scrolling and new-page continuation. Never broaden the firmware allowlist without hardware validation.

Please keep the generic public edition independent of personal profiles. New provider adapters need payload/completion tests and a documented live test before being labelled verified. Contributions are under GPL-3.0-only.
