## **MAGE threat model**

The question *"Who is MAGE for?"* is probably my least favorite answer to think of; but we'll try to decide with the messy design issues that arise when making encryption software.

**MAGE** isn't built to beat out industry standard software like **WinRAR** or **7-zip**; rather it functions similarly but **MAGE's** main goal is to ensure a 1:1 replica of what you once encrypted and are now decrypting assuming you know / have the correct entropy source. Data encrypted with **MAGE** at decrypt time must be bit-identical. Every single time. A single flipped bit in a MAGE archive makes decryption (via MAGE at least) impossible; the format is strict.

**MAGE** excels on single user systems where only *you* have authorization without a 2nd entity watching over your moves. Archives are also good on cloud storage since getting into them is near impossible assuming your password is not in `rockyou.txt`.

### **What it defends against**
**MAGE** is built to handle standard local access threats, malicious archive structures, and data tampering...
|                 Threat               |                                                             Defense                                                       |
|--------------------------------------|---------------------------------------------------------------------------------------------------------------------------|
|     **Unauthorized local access**    |                          **Argon2id + AEAD** - bruteforce is computationally expensive and memory hard.                   |
|      **Metadata reconnaissance**     |                    **Solid archive** - zero leakage without the key; paths and sizes are fully encrypted.                 |
|   **Bit drift n' archive tampering** | **Authenticated encryption** - the `header` is AAD; every chunk is MAC verified before decryption to guarantee a 1:1 bit replica. |
|     **Pre-extraction tampering**     | **Offline verification (`am_i_evil`)** - pre-validates AEAD stream authenticity, headers, and KDF bounds without writing to disk. |
|           **Path traversal**         |                **Strict canonicalization** - 22 separate validation checks kill *"Zip Slip"* attempts dead.               |
|           **Zipbomb / DoS**          |                  **Dynamic size tracking** sizes are accumulated and strictly enforced during extraction.                 |
|          **Coercion / duress**       | **Indistinguishability** - unified error messages and keyfiles give you indistinguishability of authentication failure (an attacker cannot prove if the password was wrong, a keyfile was missing; or the archive is corrupt). |
| **Memory forensics (opportunistic)** |                    **Secure allocator** - explicit wipes and `VirtualLock` / `mlock` on sensitive buffers.                |
|          **Self-sabotage**           |                        **Sanity checks** - double-encryption and self-archiving are explicitly blocked.                   |

### **What it explicitly DOES NOT defend against**
**MAGE** does not protect against the following though...
- **Long-term media bit rot reconstruction (forward error correction / PAR2)** - **MAGE** enforces strict *tamper detection*, not *error correction*. It does not embed Reed-Solomon parity or recovery records. If physical media degradation or an attacker flips bits, **MAGE** deliberately refuses to output corrupted plaintext and treats it as an integrity failure. For physical media longevity; keep separate redundant backups.
- **A compromised system** - if your system has an active RAT; any attacker can either use a keylogger or just dump your current memory for the key since it has to exist at *ONE* point.
- **Quantum computing** - okay, this is bit of a far-stretch... however! Since we use algorithms like **AES-256-GCM**, **XChaCha20-Poly1305** and so on; these technologies are meant for today's computing; not tomorrow's.
- **Physical coercion** - you could have all the mathematically perfect encryption in the world, however none of it probably means anything if you're up against the CIA being waterboarded or someone with a $5 wrench. 

### **At the end of the day**
If you want something *"better"* (in terms of more reviewed / audited), use something like **age**, **Cryptomator**, or hell even **7-zip** with a strong password will deter 99% of skids trying to snoop through your important files, documents, assets, whatever.

However for the *"paranoid"* group, the people who have a reason (or don't but they think they do); **MAGE** is a much more suitable and stricter option than anything mentioned. But then again, it is not audited; it is not *proven* to be safe yet. Most you can rely on is the half-baked codebase of ciphers and KDF implementations.