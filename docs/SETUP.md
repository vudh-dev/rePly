# Setup and recovery

The desktop app requires internet access to fetch checksum-pinned XOVI and rm-shot releases. The tablet needs Wi-Fi for AI requests. USB SSH is at `10.11.99.1`.

If connection fails, check the cable, developer mode and current on-device SSH password. If the tablet was reset, its host key may change. Verify that reset before removing the app's `device-host-key.json` from its platform application-data directory and reconnecting. Never accept an unexplained identity change.

If the interface does not start, reboot the tablet. The app only creates `/run/systemd/system/xochitl.service.d/90-reply.conf`, which disappears on reboot. It does not change the system partition, update preferences or existing permanent startup configuration. A pre-existing modification can still load after reboot.

When SSH works, the app's **Return to previous setup** button removes its temporary override and restarts the notebook interface. Manual equivalent:

```sh
sh /home/root/.local/share/reply/stop.sh
```

Installation lives under `/home/root/reply-runtime`; settings under `/home/root/.config/reply`; state under `/home/root/.local/share/reply`. Existing private Ink Diary/XOVI installations are left in place and temporarily superseded. A failed resource rebuild restores the normal system service. Keep USB attached and the tablet unlocked during indexing.

To uninstall, first return to the previous setup. Then remove only the rePly directories listed above, plus `/home/root/reply-setup` and `/home/root/reply-hash-runtime`. This removes saved API credentials and app history; it does not remove answer strokes already in notebooks. Do not remove a shared `/home/root/xovi` installation.

If no answer appears, check tablet Wi-Fi, provider billing/model access and app status. Try the toolbar wand to distinguish gesture detection from provider problems. Keep the pen away while the answer is written; changing pages can cancel writing.
