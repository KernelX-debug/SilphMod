import argparse
import hashlib
import json
import lzma
import struct
import urllib.request
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--archive", type=Path, help="Use an existing official .xz instead of downloading")
    parser.add_argument("--backend", choices=["attach", "spawn"], default="attach")
    args = parser.parse_args()
    manifest = json.loads((ROOT / "tools/native-runtime.json").read_text(encoding="utf-8"))
    if args.backend == "spawn":
        manifest = manifest["spawn"]
    version = manifest["version"]
    url = f"https://github.com/frida/frida/releases/download/{version}/frida-inject-{version}-android-arm64.xz"
    if args.archive:
        compressed = args.archive.read_bytes()
    else:
        with urllib.request.urlopen(url, timeout=60) as response:
            compressed = response.read(32 * 1024 * 1024 + 1)
        if len(compressed) > 32 * 1024 * 1024:
            raise ValueError("Archive size exceeds preparation limit")
    if hashlib.sha256(compressed).hexdigest() != manifest["archiveSha256"]:
        raise ValueError("Upstream archive SHA-256 mismatch")
    binary = lzma.decompress(compressed, memlimit=256 * 1024 * 1024)
    if len(binary) != manifest["bytes"] or hashlib.sha256(binary).hexdigest() != manifest["binarySha256"]:
        raise ValueError("Runtime bytes/SHA-256 mismatch")
    if binary[:6] != b"\x7fELF\x02\x01" or struct.unpack_from("<H", binary, 18)[0] != 183:
        raise ValueError("Expected an AArch64 ELF executable")
    library = "libsilph-spawn.so" if args.backend == "spawn" else "librootcatch-inject.so"
    target = ROOT / "app/src/main/jniLibs/arm64-v8a" / library
    target.parent.mkdir(parents=True, exist_ok=True)
    if target.exists() and target.read_bytes() != binary:
        raise ValueError("Refusing to overwrite a different runtime")
    if not target.exists():
        target.write_bytes(binary)
    print(json.dumps({"prepared": True, "bytes": len(binary), "sha256": manifest["binarySha256"]}))


if __name__ == "__main__":
    main()
