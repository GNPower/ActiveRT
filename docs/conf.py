# conf.py — Sphinx configuration for ActiveRT documentation
# Uses Breathe to bridge Doxygen XML output into Sphinx.

import os
import shutil
import subprocess
from pathlib import Path


# ---------------------------------------------------------------------------
# Auto-run Doxygen (used by Read the Docs and any bare sphinx-build invocation
# that does not pre-set DOXYGEN_XML_DIR).
# ---------------------------------------------------------------------------

def _run_doxygen() -> Path:
    """Substitute Doxyfile.in placeholders, run Doxygen, return XML output dir."""
    docs_dir  = Path(__file__).parent
    repo_root = docs_dir.parent
    doxy_out  = docs_dir / "_build" / "doxygen"
    doxy_out.mkdir(parents=True, exist_ok=True)

    version_file = repo_root / "VERSION"
    proj_version = version_file.read_text().strip() if version_file.exists() else "0.0.0"

    substitutions = {
        "@PROJECT_VERSION@":    proj_version,
        "@DOXYGEN_OUTPUT_DIR@": str(doxy_out).replace("\\", "/"),
        "@CMAKE_SOURCE_DIR@":   str(repo_root).replace("\\", "/"),
    }

    content = (docs_dir / "Doxyfile.in").read_text()
    for key, val in substitutions.items():
        content = content.replace(key, val)

    doxyfile = doxy_out / "Doxyfile"
    doxyfile.write_text(content)

    if shutil.which("doxygen") is None:
        raise RuntimeError(
            "doxygen not found on PATH. "
            "Install it (e.g. 'sudo apt install doxygen') or set DOXYGEN_XML_DIR "
            "to point at a pre-built XML directory."
        )

    subprocess.run(["doxygen", str(doxyfile)], cwd=str(repo_root), check=True)
    return doxy_out / "xml"

# ---- Project information -----------------------------------------------

project   = "ActiveRT"
author    = "Graham N. Power"
copyright = "2025, Graham N. Power"

# Version is injected by CMake via conf.py.in; fall back to reading VERSION.
try:
    _version_file = os.path.join(os.path.dirname(__file__), "..", "VERSION")
    with open(_version_file) as _f:
        release = _f.read().strip()
except OSError:
    release = "2.0.0"

version = ".".join(release.split(".")[:2])

# ---- Extensions --------------------------------------------------------

extensions = [
    "breathe",          # Doxygen XML → Sphinx
    "myst_parser",      # Markdown support (.md files as Sphinx pages)
    "sphinx_copybutton",# Copy-to-clipboard button on code blocks
]

# ---- Breathe configuration --------------------------------------------
# Priority:
#   1. DOXYGEN_XML_DIR env var  — set by build_docs.py or the CMake docs target
#   2. Auto-run Doxygen         — used by Read the Docs and bare sphinx-build

_explicit_xml = os.environ.get("DOXYGEN_XML_DIR")
_doxygen_xml_dir = Path(_explicit_xml) if _explicit_xml else _run_doxygen()

breathe_projects        = {"ActiveRT": _doxygen_xml_dir}
breathe_default_project = "ActiveRT"
breathe_default_members = ("members", "undoc-members")

# ---- MyST parser settings ----------------------------------------------

myst_enable_extensions = [
    "colon_fence",  # ::: fences as directive alternative
    "deflist",      # definition lists
]

source_suffix = {
    ".rst": "restructuredtext",
    ".md":  "myst",
}

# ---- HTML theme --------------------------------------------------------

html_theme         = "furo"
html_title         = f"ActiveRT {release}"
html_static_path   = ["_static"]
html_css_files     = ["custom.css"]

html_theme_options = {
    "sidebar_hide_name": False,
    "navigation_with_keys": True,
}

# ---- Misc --------------------------------------------------------------

master_doc     = "index"
exclude_patterns = ["_build", "Thumbs.db", ".DS_Store"]
pygments_style = "monokai"
