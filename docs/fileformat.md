# **MAGE1 file format**
The `.mage` archive format is simple on the outside but extremely strict on the inside; it uses a solid compression model, meaning the archive's metadata (file paths, timestamps, directory structures) is compressed and encrypted right alongside the actual file contents.

## **Nonce construction (TLS 1.3 RFC 8446 §5.3 pattern)**
To guarantee nonce uniqueness while preventing reordering attacks, **MAGE** constructs nonces and AAD dynamically per chunk using a monotonically increasing 64-bit `chunk_index` (starting at 0), inspired by the TLS 1.3 (RFC 8446 §5.3) sequence-number XOR masking pattern:

1. **Dynamic truncation:** the 24-byte `base nonce` (stored in the header) is first truncated to the exact size required by the active cipher (`12` bytes for **AES-GCM**, `24` bytes for **XChaCha20**, or `16` bytes for **AES-SIV**).
2. **Nonce XOR:** the 64-bit `chunk_index` is treated as an unsigned integer in **little-endian** byte order and XORed byte-by-byte into the first 8 bytes (`current_nonce[0..7]`, the lowest byte offsets) of the truncated base nonce:

```text
current_nonce[i] = base_nonce[i] ^ ((chunk_index >> (i * 8)) & 0xFF)   for i in [0..7]
```

Any remaining bytes (ex. bytes 8-11 for **AES-GCM**, or bytes 8-23 for **XChaCha20**) remain untouched from the base nonce.

3. **AAD (additional authenticated data):** every chunk authenticates the full container header alongside its own position:

```text
AAD = [65-byte header] + [8-byte chunk_index (little-endian uint64_t)]
```

This ensures that every chunk has a mathematically unique nonce, and chunks cannot be swapped, reordered, or moved between archives without MAC verification instantly failing.

## **The format itself**
The container format is split into two physical parts on disk:

1. The **header** (65 bytes): completely unencrypted so that the KDF parameters, salt, and base nonce can be read to derive the decryption key.
2. The **payload** (N bytes): a sequence of encrypted AEAD chunks of size up to `chunk_size_bytes + 16` bytes (where 16 bytes is the AEAD MAC tag). When decrypted and decompressed, it forms a continuous plaintext stream.

---

### **The header (65 bytes)**
All multi-byte integers are stored in **little-endian** byte order.

| Offset | Size (bytes) | Type | Name | Description |
|---|---|---|---|---|
| `0x00` | 4 | `char[4]` | **Magic bytes** | Always `M`, `A`, `G`, `E`. If these do not match, it is not a valid MAGE archive. |
| `0x04` | 1 | `uint8_t` | **Version** | Archive specification version (currently `1`). |
| `0x05` | 1 | `uint8_t` | **Cipher** | Encryption algorithm: `0` = AES-256-GCM, `1` = XChaCha20-Poly1305, `2` = AES-256-SIV. |
| `0x06` | 1 | `uint8_t` | **Metadata flag** | `1` if file timestamps and OS attributes are preserved, `0` if stripped. |
| `0x07` | 1 | `uint8_t` | **Compression** | Compression engine: `0` = Zstandard (ZSTD), `1` = LZMA2. |
| `0x08` | 1 | `int8_t` | **Level** | Compression preset or raw compression level (`0` = Store preset). |
| `0x09` | 4 | `uint32_t` | **Mem cost** | Argon2id memory cost in KiB (1,024 KiB to 2,097,152 KiB). |
| `0x0D` | 4 | `uint32_t` | **Time cost** | Argon2id iterations (passes over memory, 1 to 32). |
| `0x11` | 4 | `uint32_t` | **Parallelism** | Argon2id execution threads/lanes (1 to 32). |
| `0x15` | 16 | `uint8_t[16]` | **KDF salt** | Cryptographically secure random salt for Argon2id. |
| `0x25` | 24 | `uint8_t[24]` | **Base nonce** | Base IV for constructing per-chunk nonces. |
| `0x3D` | 4 | `uint32_t` | **Chunk size** | Maximum uncompressed payload bytes per AEAD chunk (1 MB to 64 MB). |

---

### **The payload stream**
Once the master key is derived via **Argon2id**, and the AEAD chunks are decrypted and decompressed, the raw plaintext stream unfolds in a single-pass sequential record format:

```text
[Stream Preamble] -> [Entry Record 1] -> [Entry Record 2] -> ... -> [Entry Record N]
```

#### **1. Stream preamble**
Right at the start of the uncompressed stream, there are two global header fields:

| Offset | Size (bytes) | Type | Name | Description |
|---|---|---|---|---|
| `0x00` | 8 | `uint64_t` | **Total origin size** | Cumulative uncompressed size of all file data combined. |
| `0x08` | 4 | `uint32_t` | **Number of entries** | Total count of file and directory records in the archive (limit: 1,000,000). |

##### **Zipbomb prevention:**
Before extracting any files, the unpacker inspects `total_origin_size`:
- If the archive's compressed size is known and `total_origin_size > compressed_size * 100` with `total_origin_size > 1 GB`, MAGE flags a possible zipbomb.
- During extraction, cumulative extracted bytes are strictly tracked against `total_origin_size`. If an entry declares or emits bytes that exceed the declared budget, extraction halts immediately.

---

#### **2. Sequential entry records**
Immediately following the preamble, all `num_entries` records appear sequentially. Each entry consists of an **entry header**, followed immediately by its **raw file payload** (if it is a non-empty file):

##### **Entry header:**
| Size (bytes) | Type | Name | Description |
|---|---|---|---|
| 2 | `uint16_t` | **Path length** | Byte length of the relative path string ($L$). |
| $L$ | `char[L]` | **Relative path** | Normalized UTF-8 relative path (e.g. `sub/document.pdf`). |
| 1 | `uint8_t` | **Is directory** | `1` if the entry is a folder, `0` if it is a regular file. |
| 8 | `uint64_t` | **File size** | Size of the file in bytes (always `0` for directories). |

*If the `metadata flag` in the 65-byte container header was `1`, the entry header includes 4 additional metadata fields:*

| Size (bytes) | Type | Name | Description |
|---|---|---|---|
| 8 | `uint64_t` | **CTime** | File creation timestamp. |
| 8 | `uint64_t` | **ATime** | File last access timestamp. |
| 8 | `uint64_t` | **MTime** | File last modification timestamp. |
| 4 | `uint32_t` | **Attributes** | Native Win32 / POSIX file attributes and permission bits. |

##### **Entry file payload:**
Immediately following the entry header:
- If `is_directory == 1` or `file_size == 0`: No payload bytes are written; the stream transitions immediately to the next entry record.
- If `is_directory == 0` and `file_size > 0`: Exactly `file_size` raw file bytes follow sequentially in the stream before the next entry begins.

Because each file's data immediately follows its metadata header, extraction is fully streaming and single-pass: the unpacker reads one entry header, streams its contents to disk (or memory for verification), and proceeds directly to the next entry without needing central directory seeking or massive RAM buffers.

## **Why not standard `.zip`?**
The legacy `.zip` format has a long history of critical security flaws, including path traversal (*zip slip*), central directory parser mismatches (*Zombie Zip*, CVE-2026-0866), and archive concatenatio n/ obfuscation (MITRE ATT&CK T1027.015).

In contrast, **MAGE** enforces:
1. **Authenticated encryption per chunk:** No unauthenticated plaintext or CRC32 checksums; any byte modification triggers a MAC failure.
2. **Encrypted solid metadata:** File names, sizes, directory structures, and attributes are hidden inside the encrypted payload.
3. **Strict path canonicalization:** 22 validation rules prevent zip slip, drive-letter escapes, UNC network paths, DOS device names and alternate data streams.

```text
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
