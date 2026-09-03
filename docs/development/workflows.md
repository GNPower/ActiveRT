# CI/CD Workflows

ActiveRT uses three GitHub Actions workflows: build and test, static
analysis, and release. All are defined in `.github/workflows/`.
Documentation is not built by Actions, Read the Docs builds it directly
from the repository.

---

## `ci.yml` - Build and Test

**Trigger:** push or pull request to `main` or `dev`

Runs on `ubuntu-latest`:

1. Install Ninja and CMake
2. `cmake --preset host-test` - configure (fetches FreeRTOS V11.2.0 and Unity v2.5.2)
3. `cmake --build --preset host-test` - compile all sources and test executables
4. `ctest --preset host-test` - run all ten test executables

A failure at any step marks the workflow as failed and blocks the PR.

---

## `static-analysis.yml` - Code Quality

**Trigger:** push or pull request to `main` or `dev`

Four parallel jobs:

### clang-format

Uses the [`jidicula/clang-format-action`](https://github.com/jidicula/clang-format-action)
action. Checks all `.c` and `.h` files in `src/` and `include/` against
the project's `.clang-format` rules. Any reformatting needed causes
the job to fail.

### cppcheck

Clones the FreeRTOS kernel headers at V11.2.0, then runs cppcheck over
`src/` with `include/`, the kernel headers, `test/posix_config`, and
`test/platform_stubs` on the include path.

The kernel headers are not optional. `activert_config.h` raises `#error`
below FreeRTOS 11.2.0, and `FreeRTOS.h` pulls in `FreeRTOSConfig.h` and
`portmacro.h`. For the same reason `preprocessorErrorDirective` is 
deliberately not suppressed, so a broken include path fails the job.

### MISRA-C

Runs `tools/misra/run_misra_check.py`, which invokes cppcheck with the
MISRA-C 2012 addon. The script fetches the kernel itself with
`--fetch-freertos`, at the same V11.2.0 the tests use. `misra_rules.txt`
is copyrighted and not committed.

All active suppressions are listed in `docs/misra_deviations.md` with
rationale. The check fails on any Mandatory violation.

The FreeRTOS version is pinned in three places:
`test/CMakeLists.txt`, `tools/misra/run_misra_check.py`, and the cppcheck
job in `static-analysis.yml`.

### clang-tidy

1. Configures the `host-test` preset to generate `compile_commands.json`
2. Runs `clang-tidy -p build/host-test` against all `src/*.c` files
3. Check options and disabled checks are in `.clang-tidy`

---

## Documentation

Documentation is built by [Read the Docs](https://activert.readthedocs.io),
there is no documentation workflow in `.github/workflows/`.

Read the Docs is driven by [`.readthedocs.yaml`](../../.readthedocs.yaml):
it installs `doxygen` from apt, installs `docs/requirements.txt`, then runs
Sphinx against `docs/conf.py`. `conf.py` invokes Doxygen itself when
`DOXYGEN_XML_DIR` is unset, which is the case on Read the Docs, so Breathe
has the XML it needs. `fail_on_warning` is `false`, so a Sphinx warning
does not fail the build.

Which branches and tags get built is configured in the Read the Docs
dashboard, not in the repository:

| Version | Source |
| --- | --- |
| `latest` | The default branch, `main`. Rebuilt automatically when a push lands there |
| `stable` | The highest activated semantic-version tag |
| `vX.Y.Z` | Created when the tag is pushed, but only built once activated under Versions in the dashboard |

The README badge tracks `latest`.

To build the same output locally, create the virtual environment described
in the README's Documentation section, then:

```bash
python tools/build_docs.py --open
python -m http.server 8000 --directory build/docs/html
```

---

## `release.yml` - GitHub Release

**Trigger:** push of a `v*.*.*` tag (e.g. `v1.0.0`)

1. Extract the version number from the tag name
2. Parse `CHANGELOG.md` to extract the section for that version
3. Create a GitHub Release with the changelog section as the release body
4. Pre-release flag is set automatically for tags containing `-`
   (e.g. `v1.1.0-rc1`)

### Making a release

Releases PR from a feature branch to `dev` to `main`, and the tag is pushed
only after the merge into `main` has completed. Tagging earlier would
publish a GitHub Release pointing at a commit that is not yet on `main`.

1. Commit the release on a feature branch, for example `feature/v1.2.0`.
   It must include the version bump in `VERSION` and in the three
   `ACTIVERT_VERSION_*` macros in `include/activert.h`, plus a matching
   `## [X.Y.Z]` section in `CHANGELOG.md` and an entry in
   `docs/changelog.rst`.
2. Open a pull request against `dev`. CI and static analysis must pass.
3. Open a pull request from `dev` against `main` and merge it. This is
   what triggers the Read the Docs build of `latest`.
4. Only now, tag `main`:

```bash
git checkout main
git pull
git tag v1.2.0
git push origin v1.2.0
```

The release workflow creates the GitHub Release automatically, using 
the `CHANGELOG.md` section for that version as the release body.

---

## Branch Strategy

| Branch | Purpose |
| --- | --- |
| `main` | Stable releases, all workflows run and releases made from here |
| `dev` | Integration branch. CI and static analysis run. No documentation is published from here |
| Feature branches | Individual changes opened as PRs against `dev` |
