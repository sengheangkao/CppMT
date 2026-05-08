#!/bin/bash
set -e

GREEN='\033[32m'
CYAN='\033[36m'
YELLOW='\033[33m'
RED='\033[31m'
RESET='\033[0m'

echo -e "${CYAN}╔══════════════════════════════════════════╗${RESET}"
echo -e "${CYAN}║   Movie Ticket System — macOS Setup      ║${RESET}"
echo -e "${CYAN}╚══════════════════════════════════════════╝${RESET}"

# ── Step 1: Homebrew ─────────────────────────────────────
echo -e "\n${YELLOW}[1/5] Checking Homebrew...${RESET}"
if ! command -v brew &>/dev/null; then
    echo "Installing Homebrew..."
    /bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"
else
    echo -e "${GREEN}  Homebrew already installed.${RESET}"
fi

# ── Step 2: CMake ────────────────────────────────────────
echo -e "\n${YELLOW}[2/5] Checking CMake...${RESET}"
if ! command -v cmake &>/dev/null; then
    brew install cmake
else
    echo -e "${GREEN}  CMake already installed: $(cmake --version | head -1)${RESET}"
fi

# ── Step 3: xlnt from source (with submodules) ───────────
echo -e "\n${YELLOW}[3/5] Building xlnt from source...${RESET}"
rm -rf /tmp/xlnt
git clone https://github.com/tfussell/xlnt.git /tmp/xlnt
cd /tmp/xlnt
git submodule update --init --recursive   # <-- this was missing!
mkdir build && cd build
cmake .. \
    -DCMAKE_BUILD_TYPE=Release \
    -DTESTS=OFF \
    -DCMAKE_POLICY_VERSION_MINIMUM=3.5
make -j$(sysctl -n hw.ncpu)
sudo make install
cd -

# ── Step 4: Create data directory ───────────────────────
echo -e "\n${YELLOW}[4/5] Setting up directories...${RESET}"
mkdir -p data
echo -e "${GREEN}  data/ directory ready.${RESET}"

# ── Step 5: Build project ───────────────────────────────
echo -e "\n${YELLOW}[5/5] Building project...${RESET}"
rm -rf build
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(sysctl -n hw.ncpu)
cd ..

echo -e "\n${GREEN}╔══════════════════════════════════════════╗${RESET}"
echo -e "${GREEN}║   Build successful!                      ║${RESET}"
echo -e "${GREEN}║   Run: ./build/MovieTicket               ║${RESET}"
echo -e "${GREEN}╚══════════════════════════════════════════╝${RESET}"
echo ""
echo -e "  Default accounts:"
echo -e "  ${CYAN}admin${RESET}  / admin123  (Admin)"
echo -e "  ${CYAN}staff1${RESET} / staff123  (Staff)"
echo -e "  ${CYAN}alice${RESET}  / alice123  (Customer)"
echo -e "  ${CYAN}bob${RESET}    / bob123    (Customer)"
echo ""
