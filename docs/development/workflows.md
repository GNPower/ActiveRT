# CI/CD Workflows

ActiveRT uses three GitHub Actions workflows plus a release workflow.
All workflows are defined in [`.github/workflows/`](../../.github/workflows/).

---

## `ci.yml` - Build and Test

**Trigger:** push or pull request to `main` or `dev`

Runs on `ubuntu-latest`:

1. Install Ninja and CMake
2. `cmake --preset host-test` - configure (fetches FreeRTOS V11.2.0 and Unity v2.5.2)
3. `cmake --build --preset host-test` - compile all sources and test executables
4. `ctest --preset host-test` - run all three test suites

A failure at any step marks the workflow as failed and blocks the PR.

---

## `static-analysis.yml` - Code Quality

**Trigger:** push or pull request to `main` or `dev`

Three parallel jobs:

### clang-format

Uses the [`jidicula/clang-format-action`](https://github.com/jidicula/clang-format-action)
action. Checks all `.c` and `.h` files in `src/` and `include/` against
the project's `.clang-format` rules. Any reformatting needed causes
the job to fail.

### cppcheck / MISRA-C

Installs cppcheck and runs `tools/misra/run_misra_check.py`. The script
invokes cppcheck with MISRA-C 2012 add-on enabled. All active
suppressions are listed in `docs/misra_deviations.md` with rationale.

### clang-tidy

1. Configures the `host-test` preset to generate `compile_commands.json`
2. Runs `clang-tidy -p build/host-test` against all `src/*.c` files
3. Check options and disabled checks are in `.clang-tidy`

---

## `docs.yml` - Documentation

**Trigger:** push to `main` or `dev` (when `include/`, `src/`, or `docs/`
change); also triggerable manually from the Actions tab.

1. Install `doxygen`, `graphviz`, `cmake`, `ninja`
2. Install Python dependencies: `pip install -r requirements.txt`
3. `cmake --preset docs` — configure docs-only build
4. `cmake --build --preset docs` — run Doxygen + Sphinx
5. Deploy `build/docs/html/` to the `gh-pages` branch via
   [`peaceiris/actions-gh-pages`](https://github.com/peaceiris/actions-gh-pages)

The published site is available at the repository's GitHub Pages URL.

---

## `release.yml` - GitHub Release

**Trigger:** push of a `v*.*.*` tag (e.g. `v1.0.0`)

1. Extract the version number from the tag name
2. Parse `CHANGELOG.md` to extract the section for that version
3. Create a GitHub Release with the changelog section as the release body
4. Pre-release flag is set automatically for tags containing `-`
   (e.g. `v1.1.0-rc1`)

### Cutting a release

After merging the release PR to `main`:

```bash
git checkout main
git pull
git tag v1.0.0
git push origin v1.0.0
```

The release workflow creates the GitHub Release automatically within
about 30 seconds.

---

## Branch Strategy

| Branch | Purpose |
| --- | --- |
| `main` | Stable releases — all workflows run; releases cut from here |
| `dev` | Integration branch — CI and static analysis run; docs deployed |
| Feature branches | Individual changes — opened as PRs against `dev` |
