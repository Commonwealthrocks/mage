// error_msg.cpp
// last updated: 29/10/2026
#include "../.hpp/error_msg.hpp"
namespace pk::error
{
    error_desc parse(const std::string &what)
    {
        if (what.empty())
        {
            return {
                "Operation failed",
                "An unknown error occurred.",
                "Please retry the operation.",
                ""};
        }
        if (what.find("cancelled by user") != std::string::npos || what.find("cancelled") != std::string::npos)
        {
            return {
                "Operation cancelled",
                "The operation was cancelled by the user.",
                "",
                what};
        }
        if (what.find("MAC verification failed") != std::string::npos ||
            what.find("authentication tag mismatch") != std::string::npos ||
            what.find("authentication tag rejected") != std::string::npos ||
            what.find("wrong password") != std::string::npos ||
            what.find("Integrity check failed") != std::string::npos ||
            what.find("Authentication failed") != std::string::npos ||
            what.find("authentication failed") != std::string::npos)
        {
            return {
                "Authentication failed",
                "The password or keyfile does not match, or the archive data has been modified or corrupted.",
                "Double check your password or verify that you selected the correct keyfile.",
                what};
        }
        if (what.find("key derivation failed") != std::string::npos)
        {
            return {
                "Key derivation failed",
                "Could not derive the encryption key, likely due to system memory constraints.",
                "Ensure your system can allocate enough memory needed.",
                what};
        }
        if (what.find("salt generation failed") != std::string::npos || what.find("nonce generation failed") != std::string::npos)
        {
            return {
                "Random generation failed",
                "Cryptographic secure random generation failed on the system.",
                "Check system entropy sources or restart the application.",
                what};
        }
        if (what.find("EVP_") != std::string::npos || what.find("crypto_aead_") != std::string::npos)
        {
            return {
                "Internal cipher failure",
                "An internal cryptographic primitive failure occurred.",
                "Ensure your system and libraries are intact.",
                what};
        }
        if (what.find("invalid magic bytes") != std::string::npos)
        {
            return {
                "Invalid archive format",
                "The selected file is not a valid MAGE archive.",
                "Make sure you selected a genuine .mage archive file.",
                what};
        }
        if (what.find("unsupported archive version") != std::string::npos)
        {
            return {
                "Unsupported archive version",
                "This archive uses a format version that this release cannot read.",
                "Upgrade MAGE to the latest version to open this archive.",
                what};
        }
        if (what.find("file too small for header") != std::string::npos)
        {
            return {
                "File truncated",
                "The file is too small to contain a valid archive header.",
                "The archive file may have been truncated during download or copy.",
                what};
        }
        if (what.find("unexpected EOF") != std::string::npos)
        {
            return {
                "Unexpected end of file",
                "The archive ended prematurely during reading or decompression.",
                "The archive file appears incomplete or truncated.",
                what};
        }
        if (what.find("corrupted chunk") != std::string::npos || what.find("tampered / corrupted chunk") != std::string::npos)
        {
            return {
                "Corrupted chunk",
                "An archive data chunk is truncated, corrupted, or tampered with.",
                "The archive failed integrity verification and cannot be extracted.",
                what};
        }
        if (what.find("limit: 1,000,000") != std::string::npos || what.find("too many files") != std::string::npos)
        {
            return {
                "Too many files",
                "The archive exceeds the safety limit of 1,000,000 files.",
                "Split files across multiple smaller archives.",
                what};
        }
        if (what.find("excessive memory cost") != std::string::npos ||
            what.find("excessive time cost") != std::string::npos ||
            what.find("invalid parallelism") != std::string::npos ||
            what.find("malicious / dangerous KDF parameters") != std::string::npos)
        {
            return {
                "Dangerous archive parameters",
                "This archive declares excessive KDF parameters (memory, passes, or threads) that exceed safety limits.",
                "Do not attempt to decrypt archives from untrusted sources.",
                what};
        }
        if (what.find("unknown cipher algorithm") != std::string::npos || what.find("unrecognized cipher") != std::string::npos)
        {
            return {
                "Unknown cipher algorithm",
                "The archive header specifies an unrecognized cipher algorithm ID.",
                "Update your MAGE client to support newer cipher algorithms.",
                what};
        }
        if (what.find("unknown compression algorithm") != std::string::npos || what.find("unrecognized compression") != std::string::npos)
        {
            return {
                "Unknown compression algorithm",
                "The archive header specifies an unrecognized compression algorithm ID.",
                "Ensure you are running the latest version of MAGE.",
                what};
        }
        if (what.find("invalid chunk size") != std::string::npos)
        {
            return {
                "Invalid chunk size",
                "The container specifies a chunk size outside the allowable 1MB - 64MB bounds.",
                "The archive header appears malformed or corrupted.",
                what};
        }
        if (what.find("possible zipbomb detected") != std::string::npos || what.find("zipbomb bypass") != std::string::npos)
        {
            return {
                "Potential zipbomb detected",
                "The archive uncompressed size drastically exceeds safe limits.",
                "Extraction was halted to protect disk space and system stability.",
                what};
        }
        if (what.find("Invalid or malicious path") != std::string::npos ||
            what.find("escapes the output directory") != std::string::npos ||
            what.find("path traversal") != std::string::npos)
        {
            return {
                "Malicious path detected",
                "An entry in the archive attempts to traverse outside the destination folder.",
                "Extraction was halted to protect system files from being overwritten.",
                what};
        }
        if (what.find("symlink or reparse point") != std::string::npos)
        {
            return {
                "Symlink security conflict",
                "Target directory or file is a symbolic link or junction, which could allow arbitrary file writes.",
                "Extract into a fresh destination folder that does not contain existing symlinks.",
                what};
        }
        if (what.find("failed to open input file") != std::string::npos)
        {
            return {
                "File read error",
                "Could not open the specified input file for reading.",
                "Verify that the file exists and is not locked exclusively by another program.",
                what};
        }
        if (what.find("failed to read complete file") != std::string::npos)
        {
            return {
                "File read error",
                "Failed to read complete file data from disk.",
                "The storage device may have encountered an I/O error or the file was modified while reading.",
                what};
        }
        if (what.find("failed to open output file") != std::string::npos ||
            what.find("failed to create output file") != std::string::npos ||
            what.find("failed to create destination directory") != std::string::npos)
        {
            return {
                "File write error",
                "Could not create or write to the destination file or directory.",
                "Ensure you have write permissions and sufficient disk space on the target drive.",
                what};
        }
        if (what.find("path conflict:") != std::string::npos || what.find("path conflict ->") != std::string::npos || what.find("path conflict") != std::string::npos)
        {
            std::string detail = what;
            auto pos = detail.find("path conflict:");
            if (pos != std::string::npos)
                detail = detail.substr(pos + 14);
            else
            {
                pos = detail.find("path conflict ->");
                if (pos != std::string::npos)
                    detail = detail.substr(pos + 16);
            }
            while (!detail.empty() && detail.front() == ' ')
                detail.erase(detail.begin());

            return {
                "Destination path conflict",
                detail.empty() ? "A file or folder with the same name already exists in the destination." : detail,
                "Choose to overwrite or select a different extraction directory.",
                what};
        }
        if (what.find("cannot be placed inside a directory") != std::string::npos ||
            what.find("The output archive cannot be placed") != std::string::npos)
        {
            return {
                "Invalid output location",
                "The output archive cannot be placed inside a directory that is currently being archived.",
                "Choose an output folder outside the directories being archived.",
                what};
        }
        if (what.find("Cannot encrypt an already encrypted MAGE file") != std::string::npos)
        {
            return {
                "Cannot re-encrypt archive",
                "Cannot encrypt a file that is already an encrypted MAGE archive.",
                "Remove the .mage file from your archive selection.",
                what};
        }
        if (what.find("invalid keyfile") != std::string::npos ||
            what.find("keyfile reading") != std::string::npos ||
            what.find("keyfile generation") != std::string::npos ||
            what.find("open keyfile") != std::string::npos ||
            what.find("from keyfile") != std::string::npos ||
            what.find("Keyfile error") != std::string::npos ||
            what.find("keyfile size") != std::string::npos)
        {
            return {
                "Keyfile error",
                what,
                "Ensure the keyfile exists, is accessible and is exactly 512 bytes.",
                what};
        }
        if (what.find("VirtualLock quota exceeded") != std::string::npos || what.find("RLIMIT_MEMLOCK") != std::string::npos)
        {
            return {
                "Memory allocation error",
                "Secure allocator failed to lock sensitive memory in RAM.",
                "Check system memory limits or run with sufficient privileges.",
                what};
        }
        if (what.find("filesystem error") != std::string::npos ||
            what.find("cannot get file size") != std::string::npos ||
            what.find("No such file or directory") != std::string::npos)
        {
            return {
                "File access error",
                what,
                "Verify that the file exists, the path is accessible and you have sufficient read permissions.",
                what};
        }
        auto semi_pos = what.find("; ");
        if (semi_pos != std::string::npos && semi_pos < 40)
        {
            std::string t = what.substr(0, semi_pos);
            std::string m = what.substr(semi_pos + 2);
            return {
                t,
                m,
                "",
                what};
        }
        return {
            "Operation failed",
            what,
            "",
            what};
    }
    error_desc parse_idk()
    {
        return {
            "Unknown error",
            "An unexpected or unhandled exception occurred during execution.",
            "Please check application logs or retry the operation.",
            "Unknown exception (...)"};
    }
    std::string format(const std::string &what)
    {
        error_desc desc = parse(what);
        if (desc.suggestion.empty())
            return desc.title + "; " + desc.message;
        return desc.title + "; " + desc.message + " " + desc.suggestion;
    }
    std::string format_idk()
    {
        auto desc = parse_idk();
        return desc.title + "; " + desc.message;
    }
}

// end