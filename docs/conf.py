# conf.py — Sphinx configuration for ActiveRT documentation
# Uses Breathe to bridge Doxygen XML output into Sphinx.

import os

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
# Doxygen XML is generated into <build>/docs/doxygen/xml by the CMake target.
# When building locally outside CMake, set DOXYGEN_XML_DIR env variable.

_doxygen_xml_dir = os.environ.get(
    "DOXYGEN_XML_DIR",
    os.path.join(os.path.dirname(__file__), "..", "build", "docs", "doxygen", "xml"),
)

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
html_static_path   = []

html_theme_options = {
    "sidebar_hide_name": False,
    "navigation_with_keys": True,
}

# ---- Misc --------------------------------------------------------------

master_doc     = "index"
exclude_patterns = ["_build", "Thumbs.db", ".DS_Store"]
pygments_style = "monokai"
