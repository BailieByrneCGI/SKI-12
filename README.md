# Department-pack case triage prototype

This local prototype turns departmental case forms into case reports for human review. The engine removes or tokenises personal identifiers, retrieves reviewed rules, and can send filtered claims and relevant excerpts to a local Hermes agent for a suggested action. Department-specific fields, labels, prompts, risk flags and rules live in interchangeable packs. `Policy/` is the included data-protection pack and the 21 forms in `Tools/Input` are fictional examples.

## Workspace

| Folder | Contents |
| --- | --- |
| `Tools/` | C++ harness, complaint forms, legislation download tools, R plugin, and tests |
| `Policy/` | Downloaded source material, provenance, checked excerpts, and reviewed rules |
| `Packs/` | Copyable template for creating another department pack |
| `Output/` | One `CASE-N` folder per complaint, the review index, and R analytics |
| `build/` | Generated CMake files and executables; recreate this after extracting the zip |

## Build and run

From the workspace root, use CMake and a C++20 compiler. These commands use MinGW Makefiles on Windows:

```powershell
cmake -S Tools -B build -G "MinGW Makefiles"
cmake --build build
ctest --test-dir build --output-on-failure
```

Validate the included pack, then process one example with the installed local Hermes CLI:

```powershell
.\build\rag-cli.exe validate-pack --pack .\Policy
.\build\rag-cli.exe run --pack .\Policy --case 21-overdue-sar.json
```

To inspect rule retrieval without Hermes, use `--offline`. To process every form in `Tools/Input`, omit `--case`. Each run updates the relevant `Output/CASE-N` folder; [Output/index.md](Output/index.md) links the current reports. The zip includes those reports, but it does not include the compiled executable or a Hermes model.

The harness writes a human-readable `report.md`, structured `findings.json`, filtered form, PII counts, and audit record for each case. `engaged` means a rule may be relevant; it is not a final finding. Hermes confidence is an uncalibrated self report, and a named human reviewer must approve any action. See the [harness guide](Tools/README.md) for the full workflow and command options.

## Add another department

Copy `Packs/_template` to a new folder, then replace its field mapping, prompt and reviewed rules. No C++ change or rebuild is required:

```powershell
Copy-Item .\Packs\_template .\Packs\HR -Recurse
.\build\rag-cli.exe validate-pack --pack .\Packs\HR
.\build\rag-cli.exe run --pack .\Packs\HR --source .\Packs\HR\examples --output .\Output\HR --offline
```

Each rule carries its own `review_action` and `required_evidence`. The pack maps department JSON fields to the shared reference, date, claim, category, organisation and outcome concepts.

## Analyse cases with R

Install [R for Windows](https://cran.r-project.org/bin/windows/base/) if needed. Then run:

```powershell
.\Tools\run_r_analysis.ps1
```

This exports a fixed set of finding fields without complaint narratives or identities, then creates count tables, PNG charts, and [Output/Analytics/report.md](Output/Analytics/report.md). The script finds R in the current user's Programs folder, Program Files, or `PATH`; `-RscriptPath` can specify another installation. The current fictional cases have no recorded human decisions. To add reviewed outcomes and, later, request an exploratory model, follow the [R plugin guide](Tools/R/README.md).

## Update legislation and rules

`Tools/pull_legislation.ps1` downloads an official HTTPS source and records its SHA-256 provenance under `Policy/sources`. `Tools/extract_pdf_text.ps1` can extract text from a downloaded PDF when `pdftotext.exe` is installed. A reviewer must check the legislation and curate clause summaries into `Policy/rules`; raw downloads are not searched as decision rules. See the [policy guide](Policy/README.md) for the source links and current rule snapshot.

## Rebuild from the zip

The distribution zip omits `build/`, `.vscode/`, and the archived `Output/archive/previous-build/` binaries. Extract it, run the build commands above, then run a case or the R analytics script. R, Hermes, CMake, and a C++ compiler are local prerequisites rather than bundled components.
