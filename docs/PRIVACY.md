# Data flow

Desktop → tablet: root SSH over USB. The SSH host fingerprint is pinned locally. Passwords and API keys are not intentionally persisted on the computer; key validation sends the key only to its selected provider. Keys remain in memory while entered. Desktop application data stores the host fingerprint and verified dependency downloads.

Tablet → provider: page screenshot, instructions and requested answer. Optional OpenAI web search/image generation can make additional paid requests. Generic sessions do not include the author's personal profiles. Advanced history features in source can persist conversation data on the tablet when explicitly configured.

Tablet storage includes the API configuration, runtime state and diagnostics; generated answers become ordinary notebook ink and may be synced by reMarkable. Provider retention and billing follow your provider account settings. This project operates no proxy service and collects no telemetry.

Configuration and backup files containing keys are root-readable (mode 600 or inside mode 700 directories). They are not encrypted at rest. Anyone with root access can read them. Remove the app's configuration and state when transferring the device, and revoke the provider key.
