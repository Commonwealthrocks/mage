# **MAGE - Make actually good encryption!**
Archive creation and encryption software made in **C++** with `Qt6`, `NaCl`, `OpenSSL`, `ZSTD`, and `libLZMA`. No bloatware or adware, free and under the **MIT** license.

## **Warning**
**MAGE** as a dedicated encryption tool is NOT audited. Meaning it is not suited for professional usage, and **MAGE** is still early into development; most of the features ARE tested a lot to make sure they are not exploitable but there will always be edge cases, use with caution like any software.

## **Features**
- "I dunno" policy, you either get nothing without the password or everything with the password.
- File / folder encryption with **AES-256-GCM** or **XChaCha20-Poly1305** only.
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

## **General tips**
In **MAGE** there are a few features that are NOT documented in the app and only here. Example, when holding down `SHIFT` or `M2` and hovering over a spinner, you can rapidly cycle through the available options for it.

In **MAGE** the `Settings` tab and `Archive creation` tab all originally rely on the saved settings in `%appdata%\MAGE\mage.json`, however only in the general settings tab can you make changes and save them globally to the `JSON` file; when creating an archive the settings you change there are only for that session.

If you are compiling **MAGE** yourself, especially with the dynamic build option, you can strip out certain baked in `Qt6` DLLs example ones for networking and TLS. Those are generally not needed to run **MAGE**.

## **Building**
To compile **MAGE** yourself you need to make sure you are on a more modern **Windows** like **Windows 10** or **Windows 11** and have **MSYS2 UCRT64** installed to pull the following libraries needed for **MAGE**...
```bash
pacman -S mingw-w64-ucrt-x86_64-gcc mingw-w64-ucrt-x86_64-cmake mingw-w64-ucrt-x86_64-ninja mingw-w64-ucrt-x86_64-qt6 mingw-w64-ucrt-x86_64-libsodium mingw-w64-ucrt-x86_64-openssl mingw-w64-ucrt-x86_64-zstd mingw-w64-ucrt-x86_64-xz
```

Once that is done, you will want to clone the repo and `cd` into the `\src` directory.
```bash
git clone https://github.com/commonwealthrocks/mage && cd mage/src/
```

And from there you can either make a dynamic or static build, for now, static builds are still in works so it is recommended you do a dynamic build like this...
```bash
mkdir build && cd build && cmake -G "Ninja" .. && ninja && strip bin/mage.exe
```

## **About Linux / UNIX**
**MAGE** and my entire knowledge consists of `Win32API` docs, however I have accounted for a good majority of the **Linux** kernel quirks and audio drivers which made me severely suicidal.

Do keep in mind, **Linux** was never tested by me, so it might work or not; who knows.

That being said, **MAGE** is not officially compiled for **Linux** yet; because it depends a lot on each setup, so for the time being I suggest getting the portable `.zip` version of the app and using emulation software like `Wine` or `Bottles`.

## **License**
**MAGE** is provided under the **MIT** license for any and all usage! View the license [here](license.txt).