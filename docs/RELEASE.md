# Release status and test checklist

This is a test release, not a claim of universal compatibility. Published betas are available at https://github.com/vudh-dev/rePly/releases.

Completed locally: ARM64 plugin build; desktop configuration/security-boundary tests; Claude payload and completion normalization; streaming parser, ink-pagination and zigzag tests; release packaging, Mac signature verification and source-app visual inspection. The packaged Mac process started without a reported console error, but automated selection of its window timed out; complete the packaged-app launch check manually. The original private pipeline was used on Paper Pro, but the new isolated public installation is a separate integration.

Before publishing as stable, test:

- A clean install on each supported firmware, plus an existing XOVI installation.
- Unlock/resource indexing, reboot/Start and Return to previous setup.
- Live OpenAI and Claude requests using the tester's own keys.
- Zigzag and wand triggers, medium Fineliner writing, cancellation and long answers across pages.
- Tables, diagrams, provider errors and no-network recovery.
- Actual launch/install on Windows, Linux, Intel Mac and Apple silicon Mac.

Unsigned beta archives are provided for these checks. Do not describe untested platforms or Claude live calls as verified. Upload source and binary archives together with SHA256SUMS and third-party notices when publishing.
