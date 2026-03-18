#!/usr/bin/env python3
"""
build_docs.py — Local documentation build helper for ActiveRT.

Replaces the CMake docs preset for platforms where Ninja or a C compiler
is unavailable (e.g. Windows without a full build toolchain).

Usage:
    python tools/build_docs.py [--open]

Options:
    --open   Open the generated index.html in the default browser when done.

Requirements:
    doxygen     on PATH
    pip install -r requirements.txt
"""

import argparse
import os
import re
import shutil
import subprocess
import sys
from pathlib import Path


REPO_ROOT   = Path(__file__).resolve().parent.parent
DOCS_DIR    = REPO_ROOT / "docs"
BUILD_DIR   = REPO_ROOT / "build" / "docs"
DOXYGEN_OUT = BUILD_DIR / "doxygen"
SPHINX_OUT  = BUILD_DIR / "html"


# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------

def check_tool(name: str) -> str:
    """Return the full path to *name* or exit with a clear message."""
    path = shutil.which(name)
    if path is None:
        print(f"ERROR: '{name}' not found on PATH.", file=sys.stderr)
        if name == "doxygen":
            print("  Install: winget install DimitriVanHeesch.Doxygen", file=sys.stderr)
            print("  Then open a new terminal so PATH is updated.", file=sys.stderr)
        elif name == "sphinx-build":
            print("  Install: pip install -r requirements.txt", file=sys.stderr)
        sys.exit(1)
    return path


def read_version() -> str:
    version_file = REPO_ROOT / "VERSION"
    if version_file.exists():
        return version_file.read_text().strip()
    return "0.0.0"


def generate_doxyfile() -> Path:
    """Substitute @VAR@ placeholders in Doxyfile.in and write to build dir."""
    doxyfile_in  = DOCS_DIR / "Doxyfile.in"
    doxyfile_out = BUILD_DIR / "Doxyfile"

    BUILD_DIR.mkdir(parents=True, exist_ok=True)

    substitutions = {
        "@PROJECT_VERSION@":  read_version(),
        "@DOXYGEN_OUTPUT_DIR@": str(DOXYGEN_OUT),
        "@CMAKE_SOURCE_DIR@": str(REPO_ROOT),
    }

    content = doxyfile_in.read_text()
    for placeholder, value in substitutions.items():
        # Normalise Windows backslashes to forward slashes for Doxygen
        content = content.replace(placeholder, value.replace("\\", "/"))

    doxyfile_out.write_text(content)
    return doxyfile_out


# ---------------------------------------------------------------------------
# Build steps
# ---------------------------------------------------------------------------

def run_doxygen(doxygen: str, doxyfile: Path) -> None:
    print("-- Running Doxygen...")
    result = subprocess.run(
        [doxygen, str(doxyfile)],
        cwd=BUILD_DIR,
    )
    if result.returncode != 0:
        print("ERROR: Doxygen failed.", file=sys.stderr)
        sys.exit(result.returncode)
    print(f"   XML output: {DOXYGEN_OUT / 'xml'}")


def run_sphinx(sphinx: str) -> None:
    print("-- Running Sphinx...")
    env = {**os.environ, "DOXYGEN_XML_DIR": str(DOXYGEN_OUT / "xml")}
    result = subprocess.run(
        [
            sphinx,
            "-b", "html",
            "-c", str(DOCS_DIR),   # conf.py location
            str(DOCS_DIR),          # source dir
            str(SPHINX_OUT),        # output dir
        ],
        env=env,
    )
    if result.returncode != 0:
        print("ERROR: Sphinx build failed.", file=sys.stderr)
        sys.exit(result.returncode)
    print(f"   HTML output: {SPHINX_OUT}")


def open_browser(html_dir: Path) -> None:
    index = html_dir / "index.html"
    if index.exists():
        import webbrowser
        webbrowser.open(index.as_uri())
    else:
        print(f"WARNING: {index} not found — cannot open browser.", file=sys.stderr)


# ---------------------------------------------------------------------------
# Entry point
# ---------------------------------------------------------------------------

def main() -> None:
    parser = argparse.ArgumentParser(description="Build ActiveRT documentation locally.")
    parser.add_argument("--open", action="store_true",
                        help="Open index.html in the default browser when done.")
    args = parser.parse_args()

    # Change to repo root so relative paths in Doxyfile resolve correctly
    os.chdir(REPO_ROOT)

    doxygen = check_tool("doxygen")
    sphinx  = check_tool("sphinx-build")

    doxyfile = generate_doxyfile()
    run_doxygen(doxygen, doxyfile)
    run_sphinx(sphinx)

    print(f"\nDocs built successfully.")
    print(f"  Open: {SPHINX_OUT / 'index.html'}")

    if args.open:
        open_browser(SPHINX_OUT)


if __name__ == "__main__":
    main()