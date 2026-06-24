// error_msg.cpp
// last updated: 17/06/2026
#include "../.hpp/error_msg.hpp"
namespace pk::error
{
    std::string format(const std::string &what)
    {
        // crypto n' kdf errors
        if (what.find("MAC verification failed") != std::string::npos)
            return "Integrity check failed; incorrect password or corrupted archive data.";
        if (what.find("key derivation failed") != std::string::npos)
            return "Cryptography error; key derivation failed (ensure sufficient RAM limits).";
        if (what.find("salt generation failed") != std::string::npos || what.find("nonce generation failed") != std::string::npos)
            return "Cryptography error; secure random generation failed.";
        if (what.find("EVP_") != std::string::npos || what.find("crypto_aead_") != std::string::npos)
            return "Cryptography error; internal cipher failure.";
        // file n' validation errors
        if (what.find("possible zipbomb detected") != std::string::npos)
            return "Security warning; potential zipbomb detected; you declined to proceed with the extraction.";
        if (what.find("invalid magic bytes") != std::string::npos)
            return "Format error; the selected file is not a valid MAGE archive.";
        if (what.find("unsupported archive version") != std::string::npos)
            return "Format error; this archive version is not supported by your client.";
        if (what.find("file too small for header") != std::string::npos)
            return "Format error; the file is too small to be a valid archive.";
        if (what.find("corrupted archive: invalid") != std::string::npos)
            return "Format error; " + what + ".";
        if (what.find("unexpected EOF") != std::string::npos)
            return "Format error; unexpected end of file reached during extraction.";
        if (what.find("corrupted chunk: too small") != std::string::npos)
            return "Format error; an archive chunk is truncated or corrupted.";
        if (what.find("limit: 1,000,000") != std::string::npos)
            return "Sanity check failed; archive exceeds the 1,000,000 file limit.";
        // paths, fuck me
        if (what.find("failed to open input file") != std::string::npos)
            return "File system error; could not open the specified input file.";
        if (what.find("failed to open output file") != std::string::npos || what.find("failed to create output file") != std::string::npos)
            return "File system error; could not create the specified output file.";
        if (what.find("path conflict:") != std::string::npos)
            return "Path conflict; " + what.substr(14); // skip "path conflict:" prefix
        if (what.find("Invalid or malicious path") != std::string::npos || what.find("escapes the output directory") != std::string::npos)
            return "Security error; " + what + ".";
        if (what.find("VirtualLock quota exceeded") != std::string::npos)
            return "Memory error; secure allocator failed to lock RAM (quota exceeded).";
        if (what.find("The output archive cannot be placed") != std::string::npos)
            // user fault errors and they have nothing to do with me
            return "User error; " + what;
        if (what.find("Cannot encrypt an already encrypted MAGE file") != std::string::npos)
            return "User error; " + what;
        // idk
        return "Unaccounted for error; " + what;
    }
    std::string format_idk()
    {
        // i REALLY don't know
        return "Unaccounted for error; an unknown exception was thrown during execution.";
    }
}

// end