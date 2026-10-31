#!/bin/bash
## mk.sh
## last updated: 31/10/2026
set -e
TESTS_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SRC_DIR="$(cd "${TESTS_DIR}/.." && pwd)"
BIN_DIR="${TESTS_DIR}/bin"
CORPUS_PATH="${TESTS_DIR}/fuzz/corpus_path"
CORPUS_HEADER="${TESTS_DIR}/fuzz/corpus_header"
cd "$TESTS_DIR"
TAR=""
FTI=5
FRMB=2048
FMLP=512
FMLH=128
FUZZ_JOBS=1
FUZZ_WORKERS=1
FUZZ_RUNS=""
DO_BUILD=0
DO_TEST=0
DO_CLEAN=0
DO_DEPS=0
DO_CORPUS=0
BOLD="\033[1m"
GREEN="\033[1;32m"
CYAN="\033[1;36m"
YELLOW="\033[1;33m"
RED="\033[1;31m"
RESET="\033[0m"
## https://patorjk.com/software/taag/#p=display&f=ANSI+Regular&t=MAGE&x=none&v=4&h=4&w=80&we=false
## ^^
## not sure if this is gonna render fine in EVERY terminal but
## it sure does render fine in mine; so why should i care?
print_banner() {
    echo -e "${CYAN}${BOLD}"
    echo " ███    ███  █████   ██████  ███████ "
    echo " ████  ████ ██   ██ ██       ██  "
    echo " ██ ████ ██ ███████ ██   ███ █████ "
    echo " ██  ██  ██ ██   ██ ██    ██ ██  "
    echo " ██      ██ ██   ██  ██████  ███████ "
    echo -e "${RESET}${BOLD} verification n' fuzzing harness thing (v0.7a)${RESET}\n"
}
show_help() {
    print_banner
    echo -e "${BOLD}Usage:${RESET}"
    echo "  bash mk.sh [opts]"
    echo ""
    echo -e "${BOLD}Build and test options:${RESET}"
    echo "  --build, -b             compile the test suite and fuzzers into ${BIN_DIR}/"
    echo "  --test, -t              build and execute test verification suite"
    echo "  --clean                 remove build directories, compiled binaries and temporary logs"
    echo "  --deps                  check and guide installation of required system packages"
    echo ""
    echo -e "${BOLD}Fuzzing options:${RESET}"
    echo "  --fuzz=path             launch the zip slip and path traversal fuzzer"
    echo "  --fuzz=header           launch the unauthenticated 65-byte container header fuzzer"
    echo "  --fuzz=both             run both path and header fuzzers concurrently"
    echo "  --corpus                print sumary statistics of discovered corpus inputs"
    echo ""
    echo -e "${BOLD}Fuzzer settings:${RESET}"
    echo "  --timeout=<sec>         per-input execution timeout in seconds (default: ${FTI})"
    echo "  --rss-limit=<mb>        memory consumption ceiling tripwire in MB (default: ${FRMB})"
    echo "  --max-len=<bytes>       maximum input byte length (default: 512 path, 128 header)"
    echo "  --jobs=<n>              number of total fuzzing jobs (default: 1)"
    echo "  --workers=<n>           number of parallel worker processes (default: 1)"
    echo "  --runs=<n>              limit execution to a fixed number of mutations"
    echo ""
    echo -e "${BOLD}Examples:${RESET}"
    echo "  bash mk.sh --test"
    echo "  bash mk.sh --build --fuzz=path"
    echo "  bash mk.sh --build --fuzz=both"
    echo "  bash mk.sh --fuzz=header --runs=500000"
    echo ""
}
chk_deps() {
    echo -e "${CYAN}Checking dependencies...${RESET}"
    local missing=0
    for cmd in clang++ cmake ninja; do
        if command -v "$cmd" >/dev/null 2>&1; then
            echo -e "  [+] Found $cmd: $(which $cmd)"
        else
            echo -e "  ${YELLOW}[-] Missing $cmd${RESET}"
            missing=1
        fi
    done
    if [ $missing -eq 1 ]; then
        echo -e "\n${YELLOW}[!] Install missing dependencies:${RESET}"
        if command -v apt-get >/dev/null 2>&1; then
            echo "    sudo apt update && sudo apt install -y clang cmake ninja-build libsodium-dev libssl-dev libzstd-dev liblzma-dev libargon2-dev"
        elif command -v pacman >/dev/null 2>&1; then
            echo "    sudo pacman -S clang cmake ninja libsodium openssl zstd xz argon2"
        elif command -v dnf >/dev/null 2>&1; then
            echo "    sudo dnf install -y clang cmake ninja-build libsodium-devel openssl-devel libzstd-devel xz-devel libargon2-devel"
        elif command -v brew >/dev/null 2>&1; then
            echo "    brew install llvm cmake ninja libsodium openssl zstd xz argon2"
        fi
        return 1
    fi
    echo -e "${GREEN}[+] All dependencies r installed.${RESET}\n"
    return 0
}
mk_all() {
    mkdir -p "$BIN_DIR"
    mkdir -p "$CORPUS_PATH"
    mkdir -p "$CORPUS_HEADER"

    echo -e "${CYAN}[*] Compiling fuzz_path harness (Clang + libFuzzer + ASan + UBSan)...${RESET}"
    clang++ -O1 -g -fsanitize=fuzzer,address,undefined \
        -I"${SRC_DIR}/.hpp" "${TESTS_DIR}/fuzz/fuzz_path.cpp" "${SRC_DIR}/.cpp/path_handler.cpp" \
        -o "${BIN_DIR}/fuzz_path"
    echo -e "${GREEN}[+] Built ${BIN_DIR}/fuzz_path${RESET}"

    echo -e "${CYAN}[*] Compiling fuzz_header harness (Clang + libFuzzer + ASan + UBSan)...${RESET}"
    clang++ -O1 -g -fsanitize=fuzzer,address,undefined \
        -I"${SRC_DIR}/.hpp" "${TESTS_DIR}/fuzz/fuzz_header.cpp" \
        -o "${BIN_DIR}/fuzz_header"
    echo -e "${GREEN}[+] Built ${BIN_DIR}/fuzz_header${RESET}"
    if command -v cmake >/dev/null 2>&1 && [ -f "${SRC_DIR}/CMakeLists.txt" ]; then
        echo -e "${CYAN}[*] Configuring and building tests...${RESET}"
        mkdir -p "${SRC_DIR}/build"
        (cd "${SRC_DIR}/build" && cmake -G Ninja -DBUILD_TESTS=ON .. >/dev/null 2>&1 && ninja test_mage) || {
            echo -e "${YELLOW}[!] Note: CMake build failed or Qt6 dynamic dependencies missing for GUI; skipping test_mage binary.${RESET}"
        }
        if [ -f "${SRC_DIR}/build/bin/test_mage" ]; then
            cp "${SRC_DIR}/build/bin/test_mage" "${BIN_DIR}/test_mage"
            echo -e "${GREEN}[+] Deployed test_mage suite executable to ${BIN_DIR}/test_mage${RESET}"
        elif [ -f "${SRC_DIR}/build/bin/test_mage.exe" ]; then
            cp "${SRC_DIR}/build/bin/test_mage.exe" "${BIN_DIR}/test_mage.exe"
            echo -e "${GREEN}[+] Deployed test_mage.exe suite executable to ${BIN_DIR}/test_mage.exe${RESET}"
        fi
    fi
    echo ""
}
run_test_suite() {
    local test_exe=""
    if [ -f "${BIN_DIR}/test_mage" ]; then
        test_exe="${BIN_DIR}/test_mage"
    elif [ -f "${BIN_DIR}/test_mage.exe" ]; then
        test_exe="${BIN_DIR}/test_mage.exe"
    elif [ -f "${SRC_DIR}/build/bin/test_mage" ]; then
        test_exe="${SRC_DIR}/build/bin/test_mage"
    elif [ -f "${SRC_DIR}/build/bin/test_mage.exe" ]; then
        test_exe="${SRC_DIR}/build/bin/test_mage.exe"
    fi
    if [ -z "$test_exe" ]; then
        echo -e "${YELLOW}[*] test_mage executable not found; building now...${RESET}"
        mk_all
        if [ -f "${BIN_DIR}/test_mage" ]; then
            test_exe="${BIN_DIR}/test_mage"
        elif [ -f "${BIN_DIR}/test_mage.exe" ]; then
            test_exe="${BIN_DIR}/test_mage.exe"
        fi
    fi

    if [ -n "$test_exe" ]; then
        echo -e "${CYAN}[*] Executing MAGE test verification suite:${RESET}\n"
        "$test_exe"
    else
        echo -e "${RED}[-] Error: could not locate compiled test_mage executable.${RESET}"
        exit 1
    fi
}
show_corpus_stats() {
    echo -e "${CYAN}[*] Fuzzing corpus statistics:${RESET}"
    local p_count=0
    local h_count=0
    [ -d "$CORPUS_PATH" ] && p_count=$(ls -1 "$CORPUS_PATH" 2>/dev/null | wc -l)
    [ -d "$CORPUS_HEADER" ] && h_count=$(ls -1 "$CORPUS_HEADER" 2>/dev/null | wc -l)
    echo -e "  [+] Path traversal corpus:  ${BOLD}${p_count}${RESET} unique seed states (${CORPUS_PATH})"
    echo -e "  [+] Header parsing corpus:  ${BOLD}${h_count}${RESET} unique seed states (${CORPUS_HEADER})"
    echo ""
}
run_fuzzer() {
    local target="$1"
    mkdir -p "$CORPUS_PATH"
    mkdir -p "$CORPUS_HEADER"

    local extra_args="-timeout=${FTI} -rss_limit_mb=${FRMB}"
    [ -n "$FUZZ_RUNS" ] && extra_args="${extra_args} -runs=${FUZZ_RUNS}"

    if [ "$target" = "path" ]; then
        if [ ! -f "${BIN_DIR}/fuzz_path" ]; then
            mk_all
        fi
        echo -e "${CYAN}[*] Fuzzying path handler...${RESET}"
        echo -e "    Binary: ${BIN_DIR}/fuzz_path"
        echo -e "    Corpus directory: ${CORPUS_PATH}"
        echo -e "    Max len: ${FMLP} bytes | RSS limit: ${FRMB} MB | Timeout: ${FTI}s\n"
        "${BIN_DIR}/fuzz_path" "$CORPUS_PATH" -max_len="${FMLP}" ${extra_args}
    elif [ "$target" = "header" ]; then
        if [ ! -f "${BIN_DIR}/fuzz_header" ]; then
            mk_all
        fi
        echo -e "${CYAN}[*] Fuzzying header...${RESET}"
        echo -e "    Binary: ${BIN_DIR}/fuzz_header"
        echo -e "    Corpus directory: ${CORPUS_HEADER}"
        echo -e "    Max len: ${FMLH} bytes | RSS limit: ${FRMB} MB | Timeout: ${FTI}s\n"
        "${BIN_DIR}/fuzz_header" "$CORPUS_HEADER" -max_len="${FMLH}" ${extra_args}
    elif [ "$target" = "both" ]; then
        if [ ! -f "${BIN_DIR}/fuzz_path" ] || [ ! -f "${BIN_DIR}/fuzz_header" ]; then
            mk_all
        fi
        echo -e "${CYAN}[*] Launching both fuzz_path and fuzz_header concurrently...${RESET}"
        echo -e "    Logs: ${TESTS_DIR}/fuzz_path.log and ${TESTS_DIR}/fuzz_header.log"
        echo -e "    Press ${BOLD}Ctrl+C${RESET} to gracefully stop both fuzzers.\n"
        "${BIN_DIR}/fuzz_path" "$CORPUS_PATH" -max_len="${FMLP}" ${extra_args} > "${TESTS_DIR}/fuzz_path.log" 2>&1 &
        PID_PATH=$!
        "${BIN_DIR}/fuzz_header" "$CORPUS_HEADER" -max_len="${FMLH}" ${extra_args} > "${TESTS_DIR}/fuzz_header.log" 2>&1 &
        PID_HDR=$!
        cleanup() {
            echo -e "\n${YELLOW}[!] Caught signal; stopping fuzzing jobs...${RESET}"
            kill $PID_PATH $PID_HDR 2>/dev/null || true
            wait $PID_PATH $PID_HDR 2>/dev/null || true
            echo -e "${GREEN}[+] Both fuzzers stopped cleanly.${RESET}"
            show_corpus_stats
            exit 0
        }
        trap cleanup SIGINT SIGTERM
        while kill -0 $PID_PATH 2>/dev/null || kill -0 $PID_HDR 2>/dev/null; do
            sleep 2
        done
        show_corpus_stats
    fi
}
do_clean() {
    echo -e "${CYAN}[*] Cleaning build artifacts and temporary files...${RESET}"
    rm -rf "${SRC_DIR}/build" "${BIN_DIR}" "${TESTS_DIR}/fuzz_path.log" "${TESTS_DIR}/fuzz_header.log" "${TESTS_DIR}"/fuzz-*.log "${TESTS_DIR}"/crash-* "${TESTS_DIR}"/leak-* "${TESTS_DIR}"/timeout-*
    echo -e "${GREEN}[+] Test workspace cleaned.${RESET}\n"
}
if [ $# -eq 0 ]; then
    show_help
    exit 0
fi
for arg in "$@"; do
    case "$arg" in
        --build|-b)
            DO_BUILD=1
            ;;
        --test|-t)
            DO_TEST=1
            ;;
        --clean)
            DO_CLEAN=1
            ;;
        --deps)
            DO_DEPS=1
            ;;
        --corpus)
            DO_CORPUS=1
            ;;
        --fuzz=*)
            TAR="${arg#*=}"
            ;;
        --timeout=*)
            FTI="${arg#*=}"
            ;;
        --rss-limit=*)
            FRMB="${arg#*=}"
            ;;
        --max-len=*)
            FMLP="${arg#*=}"
            FMLH="${arg#*=}"
            ;;
        --jobs=*)
            FUZZ_JOBS="${arg#*=}"
            ;;
        --workers=*)
            FUZZ_WORKERS="${arg#*=}"
            ;;
        --runs=*)
            FUZZ_RUNS="${arg#*=}"
            ;;
        --help|-h)
            show_help
            exit 0
            ;;
        *)
            echo -e "${RED}[-] Unknown option: $arg${RESET}"
            echo "    Run 'bash mk.sh --help' for usage."
            exit 1
            ;;
    esac
done
if [ $DO_CLEAN -eq 1 ]; then
    do_clean
fi
if [ $DO_DEPS -eq 1 ]; then
    chk_deps
fi
if [ $DO_BUILD -eq 1 ]; then
    mk_all
fi
if [ $DO_TEST -eq 1 ]; then
    run_test_suite
fi
if [ -n "$TAR" ]; then
    run_fuzzer "$TAR"
fi
if [ $DO_CORPUS -eq 1 ]; then
    show_corpus_stats
fi

## end