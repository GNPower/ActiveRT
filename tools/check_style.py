#!/usr/bin/env python3
"""
Style checker
Checks clang-format compliance across src/ and include/.
Run with --fix to auto-apply formatting.
"""

import sys
import subprocess
import argparse
import json
from pathlib import Path
from typing import List, Tuple

# Try to find clang-format (check for versioned name first)
def find_clang_format():
    """Find the clang-format executable"""
    for cmd in ['clang-format-17', 'clang-format-16', 'clang-format-15', 'clang-format']:
        try:
            subprocess.run([cmd, '--version'], capture_output=True, check=True)
            return cmd
        except (subprocess.CalledProcessError, FileNotFoundError):
            continue
    raise FileNotFoundError("clang-format not found. Please install clang-format-17 or later.")

CLANG_FORMAT_CMD = find_clang_format()

class StyleChecker:
    def __init__(self, source_dir):
        self.source_dir = Path(source_dir)
        self.search_paths = [self.source_dir / "src", self.source_dir / "include"]
        self.config_file = self.source_dir / ".clang-format"
        self.violations = []

        # Submodules to exclude from analysis
        self.excluded_dirs = [

        ]

    def find_source_files(self) -> List[Path]:
        """Find all C source and header files in src/ and include/ excluding submodules"""
        patterns = ["**/*.c", "**/*.h"]
        files = []

        for search_path in self.search_paths:
            for pattern in patterns:
                for file in search_path.glob(pattern):
                    is_excluded = False
                    for excluded in self.excluded_dirs:
                        if str(file).startswith(str(self.source_dir / excluded)):
                            is_excluded = True
                            break

                    if not is_excluded:
                        files.append(file)

        return sorted(set(files))

    def check_file_style(self, file_path: Path) -> Tuple[bool, List[str]]:
        """Check a single file for style violations"""
        cmd = [
            CLANG_FORMAT_CMD,
            f"--style=file:{self.config_file}",
            "--dry-run",
            "--Werror",
            str(file_path)
        ]

        result = subprocess.run(cmd, capture_output=True, text=True)

        if result.returncode != 0:
            # Get the diff to show what needs to be changed
            diff_cmd = [
                CLANG_FORMAT_CMD,
                f"--style=file:{self.config_file}",
                "--output-replacements-xml",
                str(file_path)
            ]
            diff_result = subprocess.run(diff_cmd, capture_output=True, text=True)

            violations = self.parse_replacements(diff_result.stdout, file_path)
            return False, violations

        return True, []

    def parse_replacements(self, xml_output: str, file_path: Path) -> List[str]:
        """Parse clang-format XML output to get violation details"""
        violations = []

        # Simple XML parsing (for production, use xml.etree.ElementTree)
        import xml.etree.ElementTree as ET

        try:
            root = ET.fromstring(xml_output)
            for replacement in root.findall('.//replacement'):
                offset = replacement.get('offset', '0')
                length = replacement.get('length', '0')

                # Calculate line number from offset
                with open(file_path, 'r') as f:
                    content = f.read()
                    line_num = content[:int(offset)].count('\n') + 1

                violations.append(f"Line {line_num}: Style violation (needs reformatting)")
        except ET.ParseError:
            # Fallback if XML parsing fails
            if "<replacement " in xml_output:
                violations.append("Style violations detected (run clang-format to fix)")

        return violations

    def generate_report(self, results: dict) -> None:
        """Generate style check report"""
        report_dir = Path("build/reports/style")
        report_dir.mkdir(parents=True, exist_ok=True)

        # JSON report
        json_report = report_dir / "style_report.json"
        with open(json_report, 'w') as f:
            json.dump(results, f, indent=2)

        # Markdown report
        md_report = report_dir / "style_report.md"
        with open(md_report, 'w', encoding='utf-8') as f:
            f.write("# Code Style Report\n\n")
            f.write(f"**Total Files:** {results['total_files']}\n")
            f.write(f"**Files with Violations:** {results['files_with_violations']}\n")
            f.write(f"**Total Violations:** {results['total_violations']}\n")
            f.write(f"**Compliance Rate:** {results['compliance_rate']:.1f}%\n\n")

            f.write("## Excluded Directories (Submodules)\n\n")
            for excluded in self.excluded_dirs:
                f.write(f"- {excluded}\n")
            f.write("\n")

            if results['violations']:
                f.write("## Violations by File\n\n")
                for file, violations in results['violations'].items():
                    f.write(f"### {file}\n\n")
                    for violation in violations:
                        f.write(f"- {violation}\n")
                    f.write("\n")

    def run_checks(self, fix=False) -> bool:
        """Run all style checks on the codebase"""
        files = self.find_source_files()

        if not files:
            print("[!] No source files found")
            return False

        print(f"[*] Checking {len(files)} files for style compliance...")
        print(f"[*] Excluding submodules: {', '.join(self.excluded_dirs)}")

        results = {
            "total_files": len(files),
            "files_with_violations": 0,
            "total_violations": 0,
            "violations": {},
            "compliance_rate": 0.0
        }

        for file in files:
            rel_path = file.relative_to(self.source_dir)
            print(f"  Checking: {rel_path}")

            passed, format_violations = self.check_file_style(file)

            if format_violations:
                results["files_with_violations"] += 1
                results["violations"][str(rel_path)] = format_violations
                results["total_violations"] += len(format_violations)
                print(f"    [FAIL] {len(format_violations)} violations found")
            else:
                print(f"    [OK] Compliant")

            # Fix formatting if requested
            if fix and format_violations:
                self.fix_formatting(file)

        # Calculate compliance rate
        if results["total_files"] > 0:
            results["compliance_rate"] = (
                (results["total_files"] - results["files_with_violations"])
                / results["total_files"] * 100
            )

        # Generate report
        self.generate_report(results)

        # Print summary
        print(f"\n[*] Style Check Summary:")
        print(f"  Total Files: {results['total_files']}")
        print(f"  Compliant Files: {results['total_files'] - results['files_with_violations']}")
        print(f"  Files with Violations: {results['files_with_violations']}")
        print(f"  Total Violations: {results['total_violations']}")
        print(f"  Compliance Rate: {results['compliance_rate']:.1f}%")

        return results["files_with_violations"] == 0

    def fix_formatting(self, file_path: Path) -> None:
        """Fix formatting issues in a file"""
        cmd = [
            CLANG_FORMAT_CMD,
            f"--style=file:{self.config_file}",
            "-i",  # In-place modification
            str(file_path)
        ]

        subprocess.run(cmd)
        print(f"    [FIXED] {file_path.relative_to(self.source_dir)}")

def main():
    parser = argparse.ArgumentParser(description="Check code style compliance")
    parser.add_argument("--source-dir", default=".", help="Source directory")
    parser.add_argument("--fix", action="store_true", help="Fix formatting issues automatically")
    parser.add_argument("--report-only", action="store_true", help="Generate report without failing")

    args = parser.parse_args()

    checker = StyleChecker(args.source_dir)
    passed = checker.run_checks(args.fix)

    if not passed and not args.report_only:
        sys.exit(1)

    sys.exit(0)

if __name__ == "__main__":
    main()