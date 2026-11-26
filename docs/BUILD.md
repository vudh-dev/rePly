# Building rePly

Use Node 22 or newer and npm. Desktop sources live in `app/`, tablet C++/QMD in `tablet/`. Dependencies are locked in `package-lock.json`.

```sh
npm ci
node node_modules/electron/install.js
npm test
npm start
```

npm versions that require lifecycle-script approval may skip Electron's download; the explicit install command above fetches its official runtime. ssh2's optional native crypto acceleration is not required.

## Tablet plugin

Use the official reMarkable Paper Pro ARM64 SDK and the XOVI source checkout at commit `2b99649f5e4fd6288be7792a8570bd16418adb70`. The author built with the ferrari 3.28 SDK (Qt 6.10.3). Obtain the SDK separately from reMarkable's developer resources; it is not redistributed here.

```sh
export SDK_ENV=/path/to/sdk/environment-setup-cortexa53-crypto-poky-linux
export XOVI_REPO=/path/to/xovi
bash tablet/build.sh
node scripts/prepare-payload.cjs
```

Use your SDK's actual environment-setup filename. `tablet/build-in-container.sh` describes the author's container mount arrangement. `tablet/Dockerfile.build` provides the host build dependencies. XOVI glue is generated during the build. `tablet/bin/reply.so` and `app/tablet-payload.tar.gz` are included for installer builds; corresponding source is in `tablet/`.

Tests for the adapter and rendering helpers can be compiled on a Qt 6 host:

```sh
c++ -std=c++17 -fPIC tablet/tests/anthropic.cpp -o /tmp/tom-anthropic $(pkg-config --cflags --libs Qt6Core Qt6Gui Qt6Network)
/tmp/tom-anthropic
```

Repeat for `stream`, `inkpagination`, and `zigzag` (run zigzag from the tablet directory for its fixtures).

## Desktop packages

```sh
node scripts/package.cjs darwin-arm64 darwin-x64 win32-x64 linux-x64 linux-arm64
```

Outputs go to `release/`. On macOS, packaging ad-hoc signs Mac apps and uses ditto to preserve bundle metadata. Build Mac packages on macOS; Windows/Linux cross-builds still require native launch testing. CI builds each platform on its own runner. Binary packages are not notarized or production-signed.

Never copy the private parent workspace into a release. Use this directory only. Run a credential and personal-data audit before publishing. Runtime dependency URLs and SHA-256 pins are in `app/dependencies.json`; update only after reviewing the upstream release and testing it on supported firmware.
