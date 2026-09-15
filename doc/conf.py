# Sphinx configuration for the Texture Workbench documentation.
# Build with `make html` in this folder (see requirements.txt).

project = "Texture Workbench"
author = "Cheng-Chin Chiang"
copyright = "2026, Cheng-Chin Chiang"

extensions = [
    "sphinx.ext.mathjax",
]

# .venv is the virtual environment from the README; its packages ship .rst files Sphinx must not read
exclude_patterns = ["_build", ".venv", "Thumbs.db", ".DS_Store"]

html_theme = "sphinx_rtd_theme"
html_theme_options = {
    "navigation_depth": 3,
}
