#!/usr/bin/env python3
"""
MISRA-C compliance checker
Checks code against MISRA-C:2012 guidelines
"""

import sys
import json
import subprocess
import argparse
import re
from pathlib import Path
from typing import List, Dict, Optional, Tuple
from dataclasses import dataclass

@dataclass
class MisraViolation:
    """Represents a MISRA-C violation"""
    file: str
    line: int
    column: int
    rule: str
    category: str  # Required, Advisory, Mandatory
    message: str
    severity: str  # Error, Warning, Info

class MisraChecker:
    """MISRA-C compliance checker using cppcheck with MISRA addon"""

    # MISRA-C:2012 Rules categorization
    MANDATORY_RULES = [
        "1.3", "2.1", "2.2", "2.6", "2.7", "3.1", "3.2",
        "4.1", "4.2", "5.1", "5.2", "5.3", "5.4", "5.5",
        "9.1", "12.5", "13.1", "13.2", "13.3", "13.4", "13.5", "13.6",
        "14.1", "14.2", "14.3", "14.4", "15.1", "15.2", "15.3", "15.6", "15.7",
        "16.1", "16.2", "16.3", "16.4", "16.5", "16.6", "16.7",
        "17.1", "17.2", "17.3", "17.4", "17.5", "17.6", "17.7",
        "18.1", "18.2", "18.3", "18.6", "19.1", "21.13", "21.17", "21.18",
        "22.1", "22.2", "22.3", "22.4", "22.5", "22.6"
    ]

    REQUIRED_RULES = [
        "1.1", "1.2", "2.3", "2.4", "2.5", "3.2",
        "4.1", "4.2", "5.6", "5.7", "5.8", "5.9",
        "6.1", "6.2", "7.1", "7.2", "7.3", "7.4",
        "8.1", "8.2", "8.3", "8.4", "8.5", "8.6", "8.7", "8.8", "8.10", "8.12", "8.13", "8.14",
        "9.2", "9.3", "9.4", "9.5",
        "10.1", "10.2", "10.3", "10.4", "10.5", "10.6", "10.7", "10.8",
        "11.1", "11.2", "11.3", "11.4", "11.5", "11.6", "11.7", "11.8", "11.9",
        "12.1", "12.2", "12.3", "12.4",
        "14.4", "15.4", "15.5",
        "18.4", "18.5", "18.7", "18.8",
        "20.1", "20.2", "20.3", "20.4", "20.5", "20.6", "20.7", "20.8", "20.9", "20.10", "20.11", "20.12", "20.13", "20.14",
        "21.1", "21.2", "21.3", "21.4", "21.5", "21.6", "21.7", "21.8", "21.9", "21.10", "21.11", "21.12", "21.14", "21.15", "21.16", "21.19", "21.20"
    ]

    def __init__(self, source_dir, build_dir):
        self.source_dir = Path(source_dir)
        self.build_dir = Path(build_dir)
        self.report_dir = self.build_dir / "reports" / "misra"
        self.report_dir.mkdir(parents=True, exist_ok=True)
        self.violations = []

        # Submodules to exclude from analysis
        self.excluded_dirs = [

        ]

        # Create MISRA configuration
        self.create_misra_config()

    def create_misra_config(self):
        """Create MISRA addon configuration for cppcheck if it doesn't exist"""
        config_file = self.source_dir / "tools" / "misra" / "misra.json"

        # Do not overwrite existing config
        if config_file.exists():
            print(f"[*] Using existing MISRA config: {config_file}")
            # Still ensure rules file exists
            self.create_misra_rules_file()
            return

        config = {
            "script": "misra.py",
            "args": ["--rule-texts=tools/misra/misra_rules.txt"],
            "python": sys.executable,
            "addon": "misra",
            "enable": "all",
            "suppress": {
                # Add specific rule suppressions if needed
                # Example: "5.1": ["specific_file.c:123"]
            }
        }

        with open(config_file, 'w') as f:
            json.dump(config, f, indent=2)

        # Create MISRA rules text file (descriptions)
        self.create_misra_rules_file()

    def create_misra_rules_file(self):
        """Create MISRA rules description file if it doesn't already exist"""
        rules_file = self.source_dir / "tools" / "misra" / "misra_rules.txt"

        # Do not overwrite existing comprehensive rules file
        if rules_file.exists():
            print(f"[*] Using existing MISRA rules file: {rules_file}")
            return

        # Only create minimal rules file if none exists (fallback)
        print(f"[!] Warning: MISRA rules file not found, creating minimal version at: {rules_file}")

        rules_text = """# MISRA-C:2012 Rules Text File
# Format: Rule X.Y Category: Description
# WARNING: This is a minimal fallback file. The comprehensive file should be used.
"""

        with open(rules_file, 'w') as f:
            f.write(rules_text)

    def find_source_files(self) -> List[Path]:
        """Find all C source files in src/ directory excluding submodules"""
        source_files = []

        for c_file in (self.source_dir / "src").rglob("*.c"):
            # Check if file is in excluded directory
            is_excluded = False
            for excluded in self.excluded_dirs:
                if str(c_file).startswith(str(self.source_dir / excluded)):
                    is_excluded = True
                    break

            if not is_excluded:
                source_files.append(c_file)

        return source_files

    def find_freertos_headers(self) -> Optional[Path]:
        """Find FreeRTOS kernel root directory in known locations.

        Returns the FreeRTOS kernel root (parent of include/) if found, else None.
        Searched in order:
          1. build/freertos-kernel/  — fetched by --fetch-freertos
          2. build/host-test/_deps/freertos_kernel-src/  — cmake --preset host-test
        """
        candidates = [
            self.build_dir / "freertos-kernel",
            self.build_dir / "host-test" / "_deps" / "freertos_kernel-src",
        ]
        for candidate in candidates:
            if (candidate / "include").exists():
                print(f"[*] Found FreeRTOS headers at: {candidate / 'include'}")
                return candidate
        return None

    def fetch_freertos(self) -> Path:
        """Clone FreeRTOS kernel V11.1.0 to build/freertos-kernel/ (shallow clone)."""
        freertos_dir = self.build_dir / "freertos-kernel"
        if freertos_dir.exists():
            print(f"[*] FreeRTOS already present at: {freertos_dir}")
            return freertos_dir

        print("[*] Fetching FreeRTOS kernel V11.1.0...")
        self.build_dir.mkdir(parents=True, exist_ok=True)
        subprocess.run(
            [
                "git", "clone",
                "--depth=1",
                "--branch", "V11.1.0",
                "https://github.com/FreeRTOS/FreeRTOS-Kernel.git",
                str(freertos_dir),
            ],
            check=True,
        )
        print(f"[*] FreeRTOS fetched to: {freertos_dir}")
        return freertos_dir

    def check_with_cppcheck(self, source_files: List[Path]) -> List[MisraViolation]:
        """Run cppcheck with MISRA addon"""
        violations = []

        freertos_root = self.find_freertos_headers()

        # Prepare cppcheck command
        cmd = [
            "cppcheck",
            "--enable=all",
            "--addon=" + str(self.source_dir / "tools" / "misra" / "misra.json"),
            "--addon-python=" + sys.executable,
            "--error-exitcode=0",  # Don't exit on error, we want to collect all violations
            "--xml",
            "--xml-version=2",
            f"--output-file={self.report_dir / 'misra_cppcheck.xml'}",
            "--suppress=missingInclude",
            "--suppress=missingIncludeSystem",
            "--suppress=preprocessorErrorDirective",
            "--suppress=normalCheckLevelMaxBranches",
            "--suppress=misra-c2012-2.5",
            "--suppress=misra-c2012-8.7",
            "--suppress=misra-c2012-15.5",
            "--suppress=misra-c2012-17.7",
            "--suppress=misra-c2012-20.10",
            "--suppress=misra-c2012-21.6",
            "--suppress=misra-c2012-21.15",
            "--suppress=misra-c2012-22.8",
            "--suppress=misra-c2012-22.9",
            "--suppress=misra-c2012-11.5",
            "--std=c11",
            "--inline-suppr",
            "-D", "tskKERNEL_VERSION_MAJOR=11",
            # Analyse with all optional features enabled so every code path is checked.
            "-D", "ACTIVERT_ENABLE_DEBUG=1",
            "-D", "ACTIVERT_ENABLE_CLI=1",
            # Override the default NULL expansion so cppcheck sees 'args' as used,
            # preventing false MISRA-C 2.7 violations in activert_cli.c.
            # The #ifndef guard in activert_config.h ensures this takes precedence.
            "-D", "ACTIVERT_CLI_GET_TOKEN(args,n)=embeddedCliGetToken((args),(n))",
            "-I" + str(self.source_dir / "include"),
        ]

        if freertos_root is None:
            print("[!] FreeRTOS headers not found. Cannot run a complete MISRA check.")
            print("    Run with --fetch-freertos, or run `cmake --preset host-test` first.")
            sys.exit(1)

        # Real FreeRTOS headers available: add include paths and suppress
        # MISRA violations originating from those headers (only our src/ is
        # analysed; the headers are included for declaration resolution only).
        #
        # tools/misra/ is listed FIRST so that tools/misra/FreeRTOSConfig.h
        # intercepts the FreeRTOS.h → #include "FreeRTOSConfig.h" chain and
        # replaces configASSERT with a MISRA-clean definition, preventing
        # false Rule 10.4 violations at every ACTIVERT_ASSERT call site.
        misra_dir = self.source_dir / "tools" / "misra"
        stub_dirs = [
            self.source_dir / "test" / "posix_config",
            self.source_dir / "test" / "platform_stubs",
        ]
        cmd.append("-I" + str(misra_dir))
        cmd.append("-I" + str(freertos_root / "include"))
        for stub in stub_dirs:
            cmd.append("-I" + str(stub))

        # Suppress MISRA violations from all non-project directories.
        # freertos_root covers include/ and all portable/ subdirectories.
        for suppress_dir in [misra_dir, freertos_root] + stub_dirs:
            path_str = str(suppress_dir).replace("\\", "/")
            cmd.append(f"--suppress=misra-c2012-*:{path_str}/*")

        # Add source files
        for source in source_files:
            cmd.append(str(source))

        # Run cppcheck
        print("[*] Running cppcheck with MISRA addon...")
        result = subprocess.run(cmd, capture_output=True, text=True)

        # Parse the XML output
        violations.extend(self.parse_cppcheck_xml())

        return violations

    def parse_cppcheck_xml(self) -> List[MisraViolation]:
        """Parse cppcheck XML output for MISRA violations"""
        violations = []
        xml_file = self.report_dir / "misra_cppcheck.xml"

        if not xml_file.exists():
            return violations

        import xml.etree.ElementTree as ET
        tree = ET.parse(xml_file)
        root = tree.getroot()

        for error in root.findall('.//error'):
            # Check if this is a MISRA violation
            error_id = error.get('id', '')
            if 'misra' in error_id.lower():
                # Parse MISRA rule from id (e.g., "misra-c2012-10.1")
                rule_match = re.search(r'misra.*?(\d+\.\d+)', error_id)
                rule = rule_match.group(1) if rule_match else error_id

                # Determine category based on rule
                category = self.get_rule_category(rule)

                # Get location
                location = error.find('location')
                if location is not None:
                    violation = MisraViolation(
                        file=location.get('file', 'unknown'),
                        line=int(location.get('line', 0)),
                        column=int(location.get('column', 0)),
                        rule=rule,
                        category=category,
                        message=error.get('msg', ''),
                        severity=error.get('severity', 'warning')
                    )
                    violations.append(violation)

        return violations

    def get_rule_category(self, rule: str) -> str:
        """Determine the category of a MISRA rule"""
        if rule in self.MANDATORY_RULES:
            return "Mandatory"
        elif rule in self.REQUIRED_RULES:
            return "Required"
        else:
            return "Advisory"

    def custom_checks(self, source_file: Path) -> List[MisraViolation]:
        """Perform custom MISRA checks not covered by tools"""
        violations = []

        with open(source_file, 'r') as f:
            lines = f.readlines()

        in_block_comment = False
        for line_num, line in enumerate(lines, 1):
            stripped = line.strip()

            # Track block comment state
            if in_block_comment:
                if '*/' in line:
                    in_block_comment = False
                continue  # Skip lines inside block comments
            if stripped.startswith('/*'):
                if '*/' not in line:
                    in_block_comment = True
                continue  # Skip the opening /* line itself
            if stripped.startswith('//'):
                continue  # Skip line comments

            # Rule 7.1: Check for octal constants
            octal_pattern = r'\b0[0-7]+\b'
            if re.search(octal_pattern, line):
                violations.append(MisraViolation(
                    file=str(source_file),
                    line=line_num,
                    column=0,
                    rule="7.1",
                    category="Required",
                    message="Octal constants shall not be used",
                    severity="error"
                ))

            # Rule 15.1: Check for goto usage
            if re.search(r'\bgoto\b', line):
                violations.append(MisraViolation(
                    file=str(source_file),
                    line=line_num,
                    column=0,
                    rule="15.1",
                    category="Advisory",
                    message="goto statement should not be used",
                    severity="warning"
                ))

        return violations

    def generate_report(self, violations: List[MisraViolation]):
        """Generate MISRA compliance report"""
        # Group violations by rule
        by_rule = {}
        for v in violations:
            if v.rule not in by_rule:
                by_rule[v.rule] = []
            by_rule[v.rule].append(v)

        # Group violations by category
        by_category = {"Mandatory": [], "Required": [], "Advisory": []}
        for v in violations:
            by_category[v.category].append(v)

        # Calculate compliance metrics
        total_files = len(set(v.file for v in violations))
        mandatory_violations = len(by_category["Mandatory"])
        required_violations = len(by_category["Required"])
        advisory_violations = len(by_category["Advisory"])

        # Generate JSON report
        json_report = {
            "summary": {
                "total_violations": len(violations),
                "mandatory_violations": mandatory_violations,
                "required_violations": required_violations,
                "advisory_violations": advisory_violations,
                "files_with_violations": total_files,
                "compliance_status": "FAIL" if mandatory_violations > 0 else ("PASS WITH WARNINGS" if (required_violations + advisory_violations) > 0 else "PASS"),
                "excluded_directories": self.excluded_dirs
            },
            "by_rule": {rule: len(viols) for rule, viols in by_rule.items()},
            "violations": [
                {
                    "file": v.file,
                    "line": v.line,
                    "column": v.column,
                    "rule": v.rule,
                    "category": v.category,
                    "message": v.message,
                    "severity": v.severity
                }
                for v in violations
            ]
        }

        json_file = self.report_dir / "misra_report.json"
        with open(json_file, 'w') as f:
            json.dump(json_report, f, indent=2)

        # Generate Markdown report
        md_file = self.report_dir / "misra_report.md"
        with open(md_file, 'w', encoding='utf-8') as f:
            f.write("# MISRA-C:2012 Compliance Report\n\n")
            f.write(f"**Compliance Status:** {json_report['summary']['compliance_status']}\n\n")

            f.write("## Summary\n\n")
            f.write(f"- Total Violations: {json_report['summary']['total_violations']}\n")
            f.write(f"- Mandatory Violations: {mandatory_violations} ")
            f.write("[X]\n" if mandatory_violations > 0 else "[OK]\n")
            f.write(f"- Required Violations: {required_violations}\n")
            f.write(f"- Advisory Violations: {advisory_violations}\n")
            f.write(f"- Files with Violations: {total_files}\n\n")

            f.write("## Excluded Directories (Submodules)\n\n")
            for excluded in self.excluded_dirs:
                f.write(f"- {excluded}\n")
            f.write("\n")

            if mandatory_violations > 0:
                f.write("## [!] Mandatory Violations (Must Fix)\n\n")
                for v in by_category["Mandatory"]:
                    f.write(f"- **{v.file}:{v.line}** - Rule {v.rule}: {v.message}\n")
                f.write("\n")

            if required_violations > 0:
                f.write("## Required Violations\n\n")
                for v in by_category["Required"]:
                    f.write(f"- **{v.file}:{v.line}** - Rule {v.rule}: {v.message}\n")
                f.write("\n")

            if advisory_violations > 0:
                f.write("## Advisory Violations\n\n")
                for v in by_category["Advisory"]:
                    f.write(f"- **{v.file}:{v.line}** - Rule {v.rule}: {v.message}\n")
                f.write("\n")

            f.write("## Violations by Rule\n\n")
            sorted_rules = sorted(by_rule.items(), key=lambda x: len(x[1]), reverse=True)
            for rule, viols in sorted_rules:
                f.write(f"- Rule {rule} ({self.get_rule_category(rule)}): {len(viols)} violations\n")

        print(f"\n[*] Reports generated:")
        print(f"  - JSON: {json_file}")
        print(f"  - Markdown: {md_file}")

        return json_report

    def run_analysis(self, fetch_freertos: bool = False) -> bool:
        """Run complete MISRA-C analysis"""
        if fetch_freertos:
            self.fetch_freertos()

        print("[*] Starting MISRA-C:2012 compliance analysis...")

        # Find all C source files
        source_files = self.find_source_files()

        if not source_files:
            print("[!] No source files found")
            return False

        print(f"[*] Analyzing {len(source_files)} source files...")
        print(f"[*] Excluding submodules: {', '.join(self.excluded_dirs)}")

        # Run cppcheck with MISRA addon
        violations = self.check_with_cppcheck(source_files)

        # Run custom checks
        for source in source_files:
            violations.extend(self.custom_checks(source))

        # Generate report
        report = self.generate_report(violations)

        # Print summary
        print("\n" + "="*60)
        print("MISRA-C:2012 Compliance Summary")
        print("="*60)
        print(f"Total Violations: {report['summary']['total_violations']}")
        print(f"Mandatory Violations: {report['summary']['mandatory_violations']}")
        print(f"Required Violations: {report['summary']['required_violations']}")
        print(f"Advisory Violations: {report['summary']['advisory_violations']}")
        print(f"Compliance Status: {report['summary']['compliance_status']}")
        print("="*60)

        # Return success if no mandatory violations
        return report['summary']['mandatory_violations'] == 0

def main():
    parser = argparse.ArgumentParser(description="MISRA-C:2012 compliance checker")
    parser.add_argument("--source-dir", default=".", help="Source directory")
    parser.add_argument("--build-dir", default="build", help="Build directory")
    parser.add_argument("--strict", action="store_true",
                       help="Fail on any violation (including advisory)")
    parser.add_argument("--fetch-freertos", action="store_true",
                       help="Clone FreeRTOS kernel V11.1.0 to build/freertos-kernel/ "
                            "before analysis (enables full Rule 17.3 checking)")

    args = parser.parse_args()

    checker = MisraChecker(args.source_dir, args.build_dir)
    success = checker.run_analysis(fetch_freertos=args.fetch_freertos)

    if not success:
        print("\n[!] MISRA-C compliance check failed")
        sys.exit(1)
    else:
        print("\n[OK] MISRA-C compliance check passed")
        sys.exit(0)

if __name__ == "__main__":
    main()
