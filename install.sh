#!/usr/bin/env bash
set -euo pipefail

REPO="uniconv/uniconv"
INSTALL_DIR="$HOME/.uniconv/bin"

info() { printf '%s\n' "$@"; }
error() { printf 'Error: %s\n' "$@" >&2; exit 1; }

detect_os() {
  case "$(uname -s)" in
    Linux*)  echo "linux" ;;
    Darwin*) echo "darwin" ;;
    *)       error "Unsupported OS: $(uname -s). Only Linux and macOS are supported." ;;
  esac
}

detect_arch() {
  case "$(uname -m)" in
    x86_64|amd64)  echo "x86_64" ;;
    aarch64|arm64) echo "aarch64" ;;
    *)             error "Unsupported architecture: $(uname -m). Only x86_64 and aarch64/arm64 are supported." ;;
  esac
}

fetch_latest_version() {
  local url="https://api.github.com/repos/${REPO}/releases/latest"
  local response

  if ! response=$(curl -fsSL "$url" 2>&1); then
    error "Failed to fetch latest release from GitHub." \
          "Check your internet connection or visit https://github.com/${REPO}/releases for manual download."
  fi

  echo "$response" | grep '"tag_name"' | sed -E 's/.*"tag_name":[[:space:]]*"([^"]+)".*/\1/'
}

download_and_install() {
  local version="$1" os="$2" arch="$3"
  local filename="uniconv-${version}-${os}-${arch}.tar.gz"
  local url="https://github.com/${REPO}/releases/download/${version}/${filename}"
  local tmpdir

  tmpdir=$(mktemp -d)
  trap 'rm -rf "$tmpdir"' EXIT

  info "Downloading uniconv ${version} for ${os}/${arch}..."
  if ! curl -fsSL -o "${tmpdir}/${filename}" "$url"; then
    error "Failed to download ${url}" \
          "Visit https://github.com/${REPO}/releases for manual download."
  fi

  info "Verifying checksum..."
  local checksum_url="https://github.com/${REPO}/releases/download/${version}/checksums.txt"
  if curl -fsSL -o "${tmpdir}/checksums.txt" "$checksum_url" 2>/dev/null; then
    local expected actual
    expected=$(grep "${filename}" "${tmpdir}/checksums.txt" | awk '{print $1}')
    if [ -n "$expected" ]; then
      if command -v sha256sum >/dev/null 2>&1; then
        actual=$(sha256sum "${tmpdir}/${filename}" | awk '{print $1}')
      elif command -v shasum >/dev/null 2>&1; then
        actual=$(shasum -a 256 "${tmpdir}/${filename}" | awk '{print $1}')
      fi
      if [ -n "${actual:-}" ] && [ "$expected" != "$actual" ]; then
        error "Checksum mismatch! Expected ${expected}, got ${actual}."
      fi
    fi
  fi

  info "Installing to ${INSTALL_DIR}..."
  mkdir -p "$INSTALL_DIR"
  tar -xzf "${tmpdir}/${filename}" -C "$INSTALL_DIR"
  chmod +x "${INSTALL_DIR}/uniconv"
}

setup_path() {
  local shell_configs=()
  local path_line="export PATH=\"${INSTALL_DIR}:\$PATH\""

  if [ -n "${ZSH_VERSION:-}" ] || [ "$(basename "${SHELL:-}")" = "zsh" ]; then
    shell_configs+=("$HOME/.zshrc")
  elif [ -n "${BASH_VERSION:-}" ] || [ "$(basename "${SHELL:-}")" = "bash" ]; then
    shell_configs+=("$HOME/.bashrc")
  fi
  shell_configs+=("$HOME/.profile")

  for config in "${shell_configs[@]}"; do
    if [ -f "$config" ] && grep -qF "$INSTALL_DIR" "$config" 2>/dev/null; then
      return 0
    fi
  done

  local target="${shell_configs[0]}"
  info "Adding ${INSTALL_DIR} to PATH in ${target}..."
  printf '\n# uniconv\n%s\n' "$path_line" >> "$target"
}

main() {
  info "uniconv installer"
  info ""

  local os arch version
  os=$(detect_os)
  arch=$(detect_arch)
  version=$(fetch_latest_version)

  info "  Platform: ${os}/${arch}"
  info "  Version:  ${version}"
  info ""

  download_and_install "$version" "$os" "$arch"
  setup_path

  info ""
  info "uniconv ${version} installed successfully!"
  info ""
  info "Restart your shell or run:"
  info "  export PATH=\"${INSTALL_DIR}:\$PATH\""
  info ""
  info "Then try:"
  info "  uniconv --version"
}

main
