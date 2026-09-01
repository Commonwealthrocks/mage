## **MAGE1 file format**
The `.mage` archive format is simple on the outside but extremely strict on the inside; it uses a solid compression model, meaning the archive's metadata (file paths, timestamps, directory structures) is compressed and encrypted right alongside the actual file contents.

This prevents metadata leaks (attackers can't even see how many files are inside) and jacks up the compression ratio since paths and structural data compress very very well together.

The format is split into two parts...

The `header` which is 65 bytes; the `header` is completely unencrypted since it holds KDF parameters and magic bytes. If it were encrypted, there'd be no possible decryption. The second part of the format is the `payload` with a size of N bytes (N for *"God knows how many"*); the `payload` is the encrypted (and optionally compressed) stream holding everything else.

## **The header (65 bytes)**
All integers are stored in **little-endian** byte order, always. Since not every device / OS / arch picks the same byte order, enforcement is needed.

| Offset |  Size (bytes) |      Type     |        Name       |                                          Description                                         |
|--------|---------------|---------------|-------------------|----------------------------------------------------------------------------------------------|
| `0x00` |       4       |    `char[4]`  |  **Magic bytes**  | Always `M`, `A`, `G`, `E`; if this is missing, then it's not a **MAGE** archive... shocking. |
| `0x04` |       1       |    `uint8_t`  |    **Version**    |                                         Currently `1`.                                       |
| `0x05` |       1       |    `uint8_t`  |     **Cipher**    |                         `0` = AES-GCM, `1` = XChaCha20, `2` = AES-SIV.                       |
| `0x06` |       1       |    `uint8_t`  | **Metadata flag** |                        `1` if timestamps / attributes are saved, `0` if not.                 |
| `0x07` |       1       |    `uint8_t`  |  **Compression**  |                                   `0` = ZSTD, `1` = LZMA2.                                   |
| `0x08` |       1       |    `int8_t`   |     **Level**     |                           the compression preset / raw level used.                           |
| `0x09` |       4       |   `uint32_t`  |   **Mem cost**    |                                   Argon2id memory cost in KB.                                |
| `0x0D` |       4       |   `uint32_t`  |   **Time cost**   |                                      Argon2id iterations.                                    |
| `0x11` |       4       |   `uint32_t`  |  **Parallelism**  |                                        Argon2id lanes.                                       |
| `0x15` |      16       | `uint8_t[16]` |   **KDF salt**    |                                   random salt for Argon2id.                                  |
| `0x25` |      24       | `uint8_t[24]` |   **Base nonce**  |                             base IV for constructing chunk nonces.                           |
| `0x3D` |       4       |   `uint32_t`  |  **Chunk Size**   |                               how many bytes per encrypted chunk.                            |

## **The payload**
Once you derive the master key using **Argon2id**, and decrypt / decompress the payload stream... what does it actually look like? Well its *mostly* of what you'd expect for a strict mathematical archive format.

### **1. Zipbomb prevention (sorta)**
Right at the start of the stream, there's a single `8-byte` integer...

$$ \text{total origin size} \quad (\text{uint64\_t}) $$

This is the uncompressed size of all files combined; before doing *anything*, the unpacker checks this to make sure you aren't about to unpack a petabyte of zeros onto your SSD; they're not really cheap nowadays.

### **2. The entry table**
Next up is the file table; first, a `4-byte` integer telling us how many things we have in the archive...

$$ \text{number of entries} \quad (\text{uint32\_t}) $$

Do note, the hardcap / limit for the max amount of entries or files in a single archive is 1,000,000 (one-million). But then; for *every* entry, it lays out the properties sequentially...

|  Size |    Type    |                        What is it?                          |
|-------|------------|-------------------------------------------------------------|
|  `2`  | `uint16_t` |   **Path length**: how long the relative path string is.    |
| `var` |  `char[]`  |     **Path**: the actual string (ex. `folder/file.txt`).    |
|  `1`  |  `uint8_t` | **Is directory**: `1` if it's a folder, `0` if it's a file. |
|  `8`  | `uint64_t` |  **File size**: size of the file (always `0` for folders).  |

*If* the `metadata flag` in the `header` was `1`, it also appends these 4 fields to the entry...

| Size |    Type    |                     What is it?                     |
|------|------------|-----------------------------------------------------|
|  `8` | `uint64_t` |             **CTime**: creation time.               |
|  `8` | `uint64_t` |              **ATime**: access time.                |
|  `8` | `uint64_t` |            **MTime**: modification time.            |
|  `4` | `uint32_t` | **Attributes**: native OS attributes / permissions. |

### **3. The File Data**
After the `entry table` is completely exhausted, the raw file data begins. 
The bytes of every single file are simply concatenated together in the **exact same order** they appeared in the `entry table`. 

Directories are *skipped* since they hold no storage mass or data. 0-byte files are also *skipped* since they hold no data to encrypt and compress. Because we know the `file size` of every entry from the table; the unpacker knows exactly how many bytes to read for each file before moving on to the next one.

That's pretty much it, when the stream runs out; it is done and dusted!


### **Why not just standard `.zip`?**
I don't think I need to explain this, the `.zip` format (legacy especially) is flawed; very flawed. It has a history of being very exploitable from path traversal (commonly named *"The Zip Slip"*), header n' parser mismatch (called *"The Zombie Zip"*), archive concatenation n' obfuscation, and so on. The list is getting too long already.

**NOTE TO YOU READING THIS!!** The `.mage` format has no current reported vulnerabilities or CVEs but that does not mean they don't exist or they are impossible to forge; this document is a glance at the format from an "on-paper" view. This format should be audited and peer-reviewed like any other format before being "recommended".

```
--

Zip Slip
CVE ID: varies ¯\_(ツ)_/¯
Source: https://security.snyk.io/research/zip-slip-vulnerability

--

Zombie Zip
CVE ID: CVE-2026-0866
Source: https://www.malwarebytes.com/blog/news/2026/03/zombie-zip-method-can-fool-antivirus-during-the-first-scan
Example: https://github.com/bombadil-systems/zombie-zip

--

Archive concatenation n' obfuscation
Framework ID: T1027.015
Source: https://attack.mitre.org/techniques/T1027/015/

--
```