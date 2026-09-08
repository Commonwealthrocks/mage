# **MAGE's key derivation function (KDF)**
How **MAGE** handles password hashing / key erivation functions with **Argon2id**!

## **The problem with passwords**
Now you've probably heard of this a billion times but when it comes to passwords; us humans suck at making them. That's why we have to always mathematically derive them with a key! It's the reason why **AES-256-GCM** or **XChaCha20-Poly1305** won't take the string as the password, rather derive it into a 32-byte key of pure entropy to feed.

### **Password encoding (UTF-8)**
What happens if you use an emoji (like 🔑) or funky **UNICODE** symbols in your password? Well legit nothing, encryption works the same.

**MAGE** reads your password from the GUI and strictly converts it into a **UTF-8** byte array before handing it over to the KDF. So your password isn't limited to just **ASCII** characters; any valid **UTF-8** string or symbol is natively supported and hashed.

## **Argon2id**
To fix this, **MAGE** (and any other socially aware software developer in 2026) uses a hashing algorithm called **Argon2id** which was the winner of the *"Password Hashing Competition"* back in 2015; and that for a good reason.

Problem with algorithms like **SHA-256 / 512**, **MD5** or even **PBKDF2** is that they require no real memory to compute. Since they rely on such simple mathematical operations; modern GPUs can eat through these functions, making millions if not billions of guesses in a single second.

Now, **Argon2id** solves this by being *intentionally slow* and memory exhausting. It forces the system you are currently using to allocate a specific amount of RAM and time cost PER guess.

The *"average"* (the *"average"* changes depending on your **Argon2id** parameters of `time_cost`, `memory_cost` and `parallelism`) **Argon2id** password hash / guess takes around half a second to a full second. And that in theory limits the amount of possible guesses to something like 24 (on a single GPU) instead of millions or billions like with **SHA**. Again, these numbers VERY vary on your parameters.

### **The parameters**
In **MAGE**, you can configure the **Argon2id** parameters yourself! Assuming you know what you are doing...

- **Memory cost**: how much RAM is required to derive the key (ex. 256 MB or 1 GB). High memory cost is what *decides* to cripple a said GPU or not.
- **Time cost (iterations)**: how many passes are made over the memory; increases the raw time it takes to compute the key.
- **Parallelism (lanes)**: how many CPU threads / cores are used to compute the hash simultaneously.

With this you can decide yourself how much an attacker (or anyone who gets your `.mage` archives) should suffer if they want to rely on bruteforcing; but then again. This is no excuse to use a weak password, people can be VERY dedicated at times.

### **The salt**
Every `.mage` archive generates a unique, 16-byte cryptographically secure random `salt` upon creation (using `BCryptGenRandom` on **Windows** or `/dev/urandom` on **UNIX**). 

This salt is stored in plain text in the archive header...
$$ \text{key} = \text{argon2id}(\text{password}, \text{salt}, \text{params}) $$

Because the salt is unique per archive; attackers cannot use precomputed *"rainbow tables"* to crack your password, nor can they attack two `.mage` files at the same time even if they share the same password.

### **Keyfiles (.mgkx)**
What if you don't trust passwords at all? What if you know a $5 wrench attack is imminent? Enter *~~Sandman~~* the keyfile!

A `.mgkx` file is exactly 512 bytes of cryptographically secure random entropy generated directly by the OS kernel. To put that into comparison... you are more likey to get struck by lightning 200 times in a ROW. Or if you want something more realistic the sun would explode sooner than you bruteforcing 512 bytes of pseudo-random data.

If you use a keyfile...
- The pure entropy of the file becomes your password.
- You can store it on a hidden USB drive and keep it physically separated from your archives.
- You can use a keyfile *only* as your source of entropy.
- If you use *both* a password and a keyfile, **MAGE** seamlessly concatenates them; `[password bytes] + [512 keyfile bytes]`.


### **Coercion resistance (indistinguishability)**
Because **MAGE** uses authenticated encryption, it verifies your `password` / `keyfile` by checking the MAC of the very first chunk of data; if the MAC fails, **MAGE** throws a unified `MAC verification failed.` error.

Here's the thing that helps us out in this case; **MAGE** simply *can't* know if a keyfile was used or not. There quite is no flag in that 65-byte `header` called `keyfile_present`.

If you are coerced into handing over your password, but you secretly used a keyfile alongside it, the attacker will type in the real password and just get a MAC failure; they have no mathematical way to prove whether...

1. You gave them a fake password.
2. The archive is randomly corrupted.
3. A hidden keyfile is required but missing.

This is what I like to call plausible bullshit.