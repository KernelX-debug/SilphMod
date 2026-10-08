import argparse
import hashlib
import json
import re
import shutil
import zipfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
ROOT_FILES = {"README.md", "LICENSE", "NOTICE", ".gitignore", ".gitattributes",
              "settings.gradle.kts", "build.gradle.kts", "gradle.properties", "gradlew", "gradlew.bat"}
EXTENSIONS = {".java", ".ts", ".cjs", ".json", ".xml", ".kts", ".py", ".svg", ".png", ".txt", ".properties"}
FOLDERS = {"app", "engine", "gradle", "docs", "tools"}
EXCLUDE = {"build", "node_modules", ".gradle", ".idea", "__pycache__", "jniLibs"}


def selected():
    paths = []
    for file in ROOT.rglob("*"):
        relative = file.relative_to(ROOT)
        if any(part in EXCLUDE for part in relative.parts) or not file.is_file():
            continue
        if file.is_symlink():
            raise ValueError("Source package cannot follow symlinks: " + str(relative))
        if len(relative.parts) == 1:
            allowed = relative.name in ROOT_FILES
        else:
            allowed = relative.parts[0] in FOLDERS and file.suffix in EXTENSIONS
            if relative.as_posix() == "gradle/wrapper/gradle-wrapper.jar":
                allowed = True
        if allowed:
            if file.suffix in {".java", ".ts", ".cjs", ".json", ".py", ".kts", ".xml"}:
                text = file.read_text(encoding="utf-8")
                if re.search(r"[A-Za-z]:[/\\]Users[/\\][^/\\\"']+", text):
                    raise ValueError("Private absolute path in source: " + str(relative))
            paths.append(file)
    return sorted(paths)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--directory", type=Path, required=True)
    parser.add_argument("--zip", type=Path, required=True)
    args = parser.parse_args()
    directory = args.directory.resolve()
    archive = args.zip.resolve()
    if directory == ROOT or ROOT in directory.parents or archive == ROOT or ROOT in archive.parents:
        raise ValueError("Outputs must be outside the build workspace")
    if directory.exists() or archive.exists():
        raise ValueError("Refusing to overwrite an existing distribution")
    paths = selected()
    if sum(p.name == "README.md" for p in paths) != 1:
        raise ValueError("Expected exactly one README")
    directory.mkdir(parents=True)
    with zipfile.ZipFile(archive, "x", zipfile.ZIP_DEFLATED) as bundle:
        for file in paths:
            relative = file.relative_to(ROOT)
            target = directory / relative
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(file, target)
            bundle.write(file, "SilphMod/" + relative.as_posix())
    with zipfile.ZipFile(archive) as bundle:
        if bundle.testzip() is not None:
            raise ValueError("ZIP integrity failure")
    print(json.dumps({"files": len(paths), "sourceBytes": sum(p.stat().st_size for p in paths),
                      "zipBytes": archive.stat().st_size,
                      "zipSha256": hashlib.sha256(archive.read_bytes()).hexdigest()}))


if __name__ == "__main__":
    main()
