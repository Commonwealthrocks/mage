# **MAGE - Make actually good encryption!**
Archive creation and encryption software made in **C++** with `Qt6`, `NaCl`, `OpenSSL`, `ZSTD`, and `libLZMA`. No bloatware or adware, free and under the **MIT** license.

## **Warning**
**MAGE** as a dedicated encryption tool is NOT audited. Meaning it is not suited for professional usage, and **MAGE** is still early into development; most of the features ARE tested a lot to make sure they are not exploitable but there will always be edge cases, use with caution like any software.

## **Documentation**
You can check out the documentation for the threat model, usage of **Argon2id** and the `MAGE1` file format through these links...
- **[File format](docs/fileformat.md)**
- **[KDF usage](docs/kdf.md)**
- **[Threat model](docs/threat_model.md)**

## **Features**
- "I dunno" policy, you either get nothing without the password or everything with the password.
- File / folder encryption with **AES-256-GCM**, **AES-256-SIV** or **XChaCha20-Poly1305** only.
- **Argon2ID** hashing / password deriviation with configurable specs.
- Proper path normalization and path sanitization to prevent traversal attacks.
- `Qt6` GUI, that hopefully doesn't blind you.
- Metadata preservation, also encrypted.
- Strict archive format (`\MAGE`) which ensures no metadata leaking.
- Compression with `ZSTD` or `LZMA2` with optional raw level configuration.
- Keyfile support and generation of 512-byte random keyfiles (`.mgkx`).
- Password + keyfiles as the same entropy source.
- Secure memory handling of passwords and **Argon2ID** hashes.
- Human readable errors. Surprising!
- Bulk file decryption! Provided the entropy source matches across all files...
- Much more for you to see in the app.

## **Obtaining MAGE yourself**
You can either get **MAGE** via the [pre-built binaries](https://github.com/Commonwealthrocks/mage/releases) for **Windows**; while for **UNIX** there is no pre-compiled binary and you will have to do it manually.

### **Windows**
To compile **MAGE** on **Windows** you need to be on a more modern version of the OS like **Windows 10** or **11**; and the recommended way to compile is via **MSYS2 UCRT64**

Install the needed packages if they are not present already; and optionally install `git` if it is not present with `pacman -S mingw-w64-ucrt-x86_64-git`.
```bash
pacman -S mingw-w64-ucrt-x86_64-gcc mingw-w64-ucrt-x86_64-cmake mingw-w64-ucrt-x86_64-ninja mingw-w64-ucrt-x86_64-qt6 mingw-w64-ucrt-x86_64-libsodium mingw-w64-ucrt-x86_64-openssl mingw-w64-ucrt-x86_64-zstd mingw-w64-ucrt-x86_64-xz
```
After that simply clone the repo and enter the `mage\src` directory...
```bash
git clone https://github.com/commonwealthrocks/mage && cd mage/src/
```
After that you can (hopefully) compile the app via `Ninja` and `CMake`...
```bash
mkdir build && cd build && cmake -G "Ninja" .. && ninja && strip bin/mage.exe
```
If you want a standalone installer too for whatever reason, after compiling make sure you have **Inno Installer** installed on your machine and run the `installer.iss` file.

### **Linux / UNIX**
For **Linux / UNIX** it varies a lot since there are a LOT of different distros and ways to compile software with the penguin; for my example I used **Debian 12 Bookworm** via the `apt` package manager.

Firstly install the needed packages / libraries if not present...
```bash
sudo apt update && sudo apt install -y build-essential cmake ninja-build qt6-base-dev libqt6svg6-dev libssl-dev libsodium-dev libzstd-dev liblzma-dev libargon2-dev
```
After that, clone the repo and enter the `mage/src` directory...
```bash
git clone https://github.com/commonwealthrocks/mage && cd mage/src/
```
And finally you should be able to compile **MAGE** into an `.AppImage`...
```bash
mkdir build && cd build && cmake -G "Ninja" .. && ninja
```
Do note the **Linux** port is not *perfect* in any way; it is not as tested like the **Windows** versions; something may break like audio, GUI re-sizing, and so on.

## **License**
**MAGE** is provided under the **MIT** license for any and all usage! View the license [here](license.txt).