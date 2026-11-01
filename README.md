# **MAGE - Make actually good encryption!**
**MAGE** is provided under the MIT license; it uses Qt6 which is licensed under the GNU LGPLv3. At runtime Qt's shared libraries are dynamically linked, ensuring **MAGE's** MIT license and Qt's LGPLv3 license remain distinct.

Each piece of software has its own separated license which you can check out *[here](./3rd_party_licenses/)*.

## **Warning**
**MAGE** is NOT audited nor declared as formal encryption software; this software is suitable for personal and informal environments. It should not be used in any enterprise or professional setting due to the lack of an audit, same applies to any encryption software.

Another note to you reading this: **MAGE** is still very much in alpha. New bugs, edge cases, and optimization issues will always be found and I try my best to patch them out. There is no "stable" release of **MAGE** yet.

## **Documentation**
To learn more about the `MAGE1` format, usage of Argon2id, the threat model, and the general philosophy of the software itself, you can consult these pages...
- *[File format](docs/fileformat.md)*
- *[Password hashing (Argon2id)](docs/kdf.md)*
- *[Threat model](docs/threat_model.md)*

## **What is MAGE?**
**MAGE** is free, open source, and fully local encryption and archival software intended for personal cold storage, single-user systems, and cloud backups where you want a strict "I dunno" policy: file names, directory layout, sizes and payload contents are completely concealed.

Most archival tools treat security as an afterthought slapped onto a 30-year-old container specification; **MAGE** was designed in reverse, starting with mathematical strictness and building a container format around modern cryptography. Every archive decrypted with **MAGE** must produce a bit-identical 1:1 replica of what you packed; a single flipped bit or reordered chunk triggers an immediate MAC failure and halts extraction.

## **Key features**

### **AEAD ciphers only**
In **MAGE**, the only provided cipher options are all fully authenticated (AEAD)...
- **AES-256-GCM**: standardized under *[FIPS 197](https://nvlpubs.nist.gov/nistpubs/FIPS/NIST.FIPS.197-upd1.pdf)* and *[NIST SP 800-38D](https://nvlpubs.nist.gov/nistpubs/Legacy/SP/nistspecialpublication800-38d.pdf)* with AES-NI hardware acceleration.
- **AES-256-SIV**: standardized under *[IETF RFC 5297](https://datatracker.ietf.org/doc/html/rfc5297)*; provides synthetic initialization vector authenticated encryption with nonce *misuse resistance* (a repeated nonce only reveals whether plaintexts were identical, without leaking the key or allowing forged plaintexts).
- **XChaCha20-Poly1305**: specified under the *[IRTF CFRG draft](https://datatracker.ietf.org/doc/html/draft-irtf-cfrg-xchacha-03)* with an extended 24-byte (192-bit) nonce to eliminate nonce-collision risks when generating random nonces, as popularized by Libsodium (NaCl).

### **Memory-hard key derivation (Argon2id)**
Passwords aren't fed directly to ciphers; they are derived through Argon2id which is standardized under *[IETF RFC 9106](https://www.rfc-editor.org/info/rfc9106/)* and recognized as the *[OWASP Gold Standard](https://cheatsheetseries.owasp.org/cheatsheets/Password_Storage_Cheat_Sheet.html)*.

Each Argon2id key is derived with a unique 16-byte cryptographically secure random salt generated via `BCryptGenRandom` on Windows and `/dev/urandom` (or NaCl's `randombytes_buf()` as fallback) on POSIX-like machines.

In **MAGE** you are allowed, and in fact are encouraged, to tune Argon2id's parameters...
- **Memory cost**: how much RAM Argon2id allocates per hash calculation (1 MB to 2 GB).
- **Time cost**: how many iterative passes the algorithm performs over the memory table.
- **Parallelism**: how many independent compute threads the algorithm splits work across.

Recommended default Argon2id parameters (balanced for daily desktop use)...
- *96 MB memory cost*
- *4 iterations (time cost)*
- *2 cores (parallelism)*

### **Strict solid archive format**
The `MAGE1` container header is strictly 65 bytes (containing only the public KDF parameters and magic bytes required to compute the key):
- All file paths, directory trees, timestamps, original file sizes and OS attributes are stored *inside* the encrypted payload.
- File names, paths, sizes and folder hierarchies are completely encrypted within the solid stream. An external observer only sees public container parameters necessary for KDF derivation (cipher ID, Argon2id parameters, chunk size and salt).

Because of how strict the `MAGE1` container design is, it intentionally enforces the following...
- **No random access:** when decrypting a `.mage` file you extract the complete tree; you cannot selectively leak or stream individual files without authenticating the entire container.
- **Strict tamper rejection (no error correction):** every chunk possesses its own independent AEAD MAC authentication tag bound to a monotonic chunk index and header AAD. If even a single bit is altered or reordered anywhere in the archive, decryption terminates immediately; **MAGE** deliberately refuses to emit corrupt plaintext.

### **Keyfiles (`.mgkx`) and coercion resistance**
In **MAGE** you can generate a 512-byte random keyfile via the OS CSPRNG APIs mentioned in the Argon2id section. Bruteforcing 512 bytes of CSPRNG entropy is mathematically impossible; the universe would experience heat death long before an attacker makes *ANY* progress.

In **MAGE** you can use a password both, keyfile only, or both! An attacker cannot know what entropy source was used during the encryption of an archive, that means they cannot tell if...
- You used a password only.
- You used a keyfile only.
- You used both.
- Or the archive is simply corrupt.

An attacker attempting authentication without the required keyfile receives the exact same unified `MAC verification failed` error.

### **Archive verification**
With the built-in "Verify archive" function; you do not need to extract an archive to disk to verify it!

In the verification dialog, you can inspect `.mage` archives without passwords to review header sanity, file sizes, KDF parameters and chunk geometry. Supplying the archive's entropy source allows full in-memory AEAD stream verification to guarantee cryptographic authenticity without touching disk storage.

Results can also be exported to an audit log.

### **Zip slip n' path traversal immunity**
Archive paths are sanitized through 22 rigorous canonicalization rules...
- Kills directory traversals (`../`, `..\`) dead.
- Blocks Windows drive letters (`C:\`), UNC network roots (`\\server\share`), DOS reserved names (`CON`, `PRN`, `AUX`, `NUL`, `COM1-9`, `LPT1-9`) and NTFS alternate data streams (`file:stream`).

### **Integrated streaming compression**
**MAGE** includes streaming compression using Zstandard (ZSTD) or LZMA2 (the same engine powering 7-zip). You can select between curated presets (*"Store"*, *"Normal"*, *"Good"*, *"ULTRAKILL"*) or configure raw compression levels per algorithm.

### **Memory hygiene**
Plaintext buffers and derived cryptographic keys are wiped from RAM using volatile secure zeroing (`pk::mem_::secure_wipe` via `RtlSecureZeroMemory` on Windows and `explicit_bzero` on POSIX) and pinned via OS page locking (`VirtualLock` / `mlock`) where possible.

## **Obtaining MAGE**
The simplest way to obtain **MAGE** on Windows is via the *[pre-built binaries](https://github.com/Commonwealthrocks/mage/releases/)*.

If you wish to compile **MAGE** yourself from source, follow the instructions below.

### **Windows (MSYS2 UCRT64)**
To compile on Windows, install *[MSYS2](https://msys2.org/)* and open the `MSYS2 UCRT64` terminal:

1. Install required libraries...
```bash
pacman -S --needed --noconfirm mingw-w64-ucrt-x86_64-gcc mingw-w64-ucrt-x86_64-cmake \
                   mingw-w64-ucrt-x86_64-ninja mingw-w64-ucrt-x86_64-qt6 \
                   mingw-w64-ucrt-x86_64-libsodium mingw-w64-ucrt-x86_64-openssl \
                   mingw-w64-ucrt-x86_64-zstd mingw-w64-ucrt-x86_64-xz \
                   mingw-w64-ucrt-x86_64-argon2 \
                   mingw-w64-ucrt-x86_64-git
```

2. Clone and build...
```bash
git clone https://github.com/commonwealthrocks/mage && cd mage/src
mkdir build && cd build
cmake -G "Ninja" ..
ninja
strip bin/mage.exe
```

---

### **Linux / WSL (Debian 12 Bookworm / Ubuntu)**
**MAGE** is primarily developed on Windows; Linux builds are supported via CMake and can be built either with the Qt6 GUI or as a headless cryptographic core and test suite.

Install the needed libraries too...
```bash
sudo apt update && sudo apt install -y build-essential clang cmake ninja-build \
    libssl-dev libsodium-dev libzstd-dev liblzma-dev libargon2-dev zlib1g-dev \
    qt6-base-dev libqt6svg6-dev git
```

Then clone and build...
```bash
git clone https://github.com/commonwealthrocks/mage && cd mage/src
mkdir build_linux && cd build_linux
cmake -G "Ninja" ..
ninja
```

## **Tests n' fuzzing**
**MAGE** includes a test suite covering known answer tests (KATs), chunk tampering, boundary conditions, zip slip defenses and in-process fuzzing...
```bash
cd src/tests
bash mk.sh --test            ## build and run the tests
```

For continuous fuzzing, the script includes libFuzzer + ASan harnesses (requires `clang++`; typically run via Linux or WSL)...
```bash
bash mk.sh --fuzz=path       ## launch libFuzzer path traversal harness
bash mk.sh --fuzz=header     ## launch libFuzzer header parser harness
bash mk.sh --fuzz=both       ## run both fuzzers concurrently
```

## **Release signatures**
As of `v0.7a` for **MAGE**, each release will be signed with my GPG key and can be verified through *[my public GPG key](https://github.com/Commonwealthrocks.gpg)*. Typically releases will come with `.asc` files so you won't need to manually input my public GPG key.

By all odds, you should *NOT* download a release that has a faulty GPG signature or does not contain one; even if it's from me.
```bash
gpg --verify file_you_downloaded.ext file_you_downloaded.ext.asc
```