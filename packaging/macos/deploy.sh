#!/usr/bin/env bash
# Deploy a staging copy of Ragtime.app and sign it for local use.
set -euo pipefail

app="${1:?Usage: packaging/macos/deploy.sh path/to/staged/Ragtime.app}"
test -x "$app/Contents/MacOS/Ragtime"
qtbase="$(brew --prefix qtbase)"
wxlib="$(brew --prefix wxwidgets)/lib"
frameworks="$app/Contents/Frameworks"

tools=()
if otool -L "$app/Contents/MacOS/Ragtime" | grep libhamlib >/dev/null; then
    for tool in rigctl rigctld; do
        cp "$(brew --prefix hamlib)/bin/$tool" "$app/Contents/MacOS/$tool"
        chmod u+w "$app/Contents/MacOS/$tool"
        tools+=("-executable=$app/Contents/MacOS/$tool")
    done
fi
"$qtbase/bin/macdeployqt" "$app" -no-plugins -always-overwrite ${tools[@]+"${tools[@]}"}

# wx-config links the short dylib names, while wx's own libraries may
# refer to the fully versioned names. Load only one copy of each library.
for source in "$wxlib"/libwx*.dylib; do
    alias="${source##*/}"
    resolved="$(realpath "$source")"
    library="${resolved##*/}"
    if [[ "$alias" != "$library" && -f "$frameworks/$alias" && -f "$frameworks/$library" ]]; then
        ln -sfn "$library" "$frameworks/$alias"
    fi
done

# Normalize copied libraries' install IDs as well as their load commands.
# macdeployqt leaves some transitive Homebrew libraries with absolute IDs.
python3 - "$app" <<'PY_DEPENDENCIES'
from pathlib import Path
import re
import subprocess
import sys
app = Path(sys.argv[1]).resolve()
frameworks = app / "Contents/Frameworks"
magic = {bytes.fromhex(x) for x in ("cffaedfe", "feedfacf", "cafebabe", "bebafeca")}
seen = set()
for candidate in app.rglob("*"):
    if not candidate.is_file():
        continue
    binary = candidate.resolve()
    if binary in seen:
        continue
    if not binary.is_relative_to(app):
        raise RuntimeError(f"Library symlink leaves the app: {candidate}")
    with binary.open("rb") as source:
        if source.read(4) not in magic:
            continue
    seen.add(binary)
    install_ids = subprocess.check_output(["otool", "-D", str(binary)], text=True).splitlines()[1:]
    if install_ids:
        relative = binary.relative_to(frameworks) if binary.is_relative_to(frameworks) else None
        if relative:
            subprocess.run(["install_name_tool", "-id", "@rpath/" + str(relative), str(binary)], check=True)
    for line in subprocess.check_output(["otool", "-L", str(binary)], text=True).splitlines()[1:]:
        dependency = line.strip().split(" (", 1)[0]
        if not dependency.startswith("/") or dependency.startswith(("/System/Library/", "/usr/lib/")):
            continue
        relative = dependency.split("/lib/", 1)[-1]
        target = frameworks / relative
        if not target.exists():
            target = frameworks / Path(dependency).name
        if not target.exists() or not target.resolve().is_relative_to(app):
            raise RuntimeError(f"Unbundled dependency in {binary.name}: {dependency}")
        replacement = "@rpath/" + str(target.relative_to(frameworks))
        subprocess.run(["install_name_tool", "-change", dependency, replacement, str(binary)], check=True)
    loads = subprocess.check_output(["otool", "-l", str(binary)], text=True)
    for path in re.findall(r"\bpath (.*?) \(offset \d+\)", loads):
        if path.startswith("/"):
            subprocess.run(["install_name_tool", "-delete_rpath", path, str(binary)], check=True)
PY_DEPENDENCIES

codesign --force --deep --sign - "$app"
codesign --verify --deep --strict "$app"
