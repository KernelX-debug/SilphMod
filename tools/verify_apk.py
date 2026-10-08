import argparse
import hashlib
import json
import os
import re
import subprocess
import xml.etree.ElementTree as ET
import zipfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PERMISSIONS = {"android.permission.FOREGROUND_SERVICE", "android.permission.FOREGROUND_SERVICE_SPECIAL_USE",
               "android.permission.POST_NOTIFICATIONS", "android.permission.WAKE_LOCK"}


def run(command):
    result = subprocess.run(command, check=True, capture_output=True, text=True,
                            encoding="utf-8", errors="replace")
    return result.stdout + result.stderr


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("apk", type=Path)
    parser.add_argument("--build-tools", type=Path, help="Android SDK build-tools directory")
    args = parser.parse_args()
    sdk = os.environ.get("ANDROID_HOME") or os.environ.get("ANDROID_SDK_ROOT")
    tools = args.build_tools or (Path(sdk) / "build-tools/36.0.0" if sdk else None)
    if not tools:
        raise ValueError("Set ANDROID_HOME or use --build-tools")
    windows = os.name == "nt"
    badging = run([str(tools / ("aapt.exe" if windows else "aapt")), "dump", "badging", str(args.apk)])
    assert "name='dev.rootcatch.ugphonetest' versionCode='20' versionName='1.5.0-ugphone'" in badging
    assert "application-label:'SilphMod'" in badging
    assert "sdkVersion:'29'" in badging and "targetSdkVersion:'36'" in badging
    assert "application-debuggable" not in badging
    assert set(re.findall(r"uses-permission: name='([^']+)'", badging)) == PERMISSIONS
    signing = [str(tools / ("apksigner.bat" if windows else "apksigner")),
               "verify", "--verbose", "--print-certs", str(args.apk)]
    signature = run(signing)
    certificate = re.search(r"Signer #1 certificate SHA-256 digest: ([a-f0-9]{64})", signature).group(1)
    native = json.loads((ROOT / "tools/native-runtime.json").read_text(encoding="utf-8"))
    agent = ROOT / "app/build/generated/engine/assets/engine/agent.js"
    agent_hash = hashlib.sha256(agent.read_bytes()).hexdigest()
    with zipfile.ZipFile(args.apk) as bundle:
        assert bundle.testzip() is None and len(bundle.namelist()) == len(set(bundle.namelist()))
        assert bundle.read("assets/engine/agent.js") == agent.read_bytes()
        assert hashlib.sha256(bundle.read("lib/arm64-v8a/librootcatch-inject.so")).hexdigest() == native["binarySha256"]
        assert not any(name.endswith(".so") and name != "lib/arm64-v8a/librootcatch-inject.so" for name in bundle.namelist())
        for license_file in (ROOT / "app/src/main/assets/licenses").iterdir():
            assert bundle.read("assets/licenses/" + license_file.name) == license_file.read_bytes()
        payload = bundle.read("assets/engine/agent.js")
        assert b"__agentTest" not in payload and b"__watchdogTest" not in payload
        dex = b"".join(bundle.read(name) for name in bundle.namelist() if re.fullmatch(r"classes\d*\.dex", name))
        for label in (b"Start service", b"Export logs", b"Keep shiny", b"Keep 15/15/15", b"Fix error 12"):
            assert label in dex
        for label in ("Iniciar servicio", "Exportar logs", "Máximo por sesión", "Máximo eliminado por sesión"):
            assert label.encode("utf-8") not in dex
    generated = ROOT / "app/build/generated/engine/java/dev/rootcatch/ugphone/AgentBuild.java"
    assert agent_hash in generated.read_text(encoding="utf-8")
    totals = dict(tests=0, failures=0, errors=0, skipped=0)
    suites = list((ROOT / "app/build/test-results/testDebugUnitTest").glob("TEST-*.xml"))
    for file in suites:
        suite = ET.parse(file).getroot()
        for key in totals:
            totals[key] += int(suite.get(key, "0"))
    assert suites and totals["tests"] >= 125 and totals["failures"] == totals["errors"] == 0
    lint = ET.parse(ROOT / "app/build/reports/lint-results-release.xml").getroot()
    assert not list(lint)
    print(json.dumps({"version": "1.5.0-ugphone", "versionCode": 20, "debuggable": False,
                      "apkBytes": args.apk.stat().st_size,
                      "apkSha256": hashlib.sha256(args.apk.read_bytes()).hexdigest(),
                      "agentSha256": agent_hash, "certificateSha256": certificate,
                      "signatureV2Verified": "v2): true" in signature,
                      "signatureV3Verified": "v3): true" in signature,
                      "jvm": totals, "lintIssues": 0,
                      "permissions": sorted(PERMISSIONS), "nativeDeviceTestPerformed": False}, indent=2))


if __name__ == "__main__":
    main()
