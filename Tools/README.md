# Complaint RAG workspace

The workspace has four main folders:

| Folder | Purpose |
| --- | --- |
| `build/` | CMake build output and the `rag-cli.exe` executable |
| `Policy/` | Official source downloads, provenance, and reviewer-curated clause rules |
| `Output/` | Human-readable case reports, confidence, actions, JSON findings, and audit records |
| `Tools/` | C++ harness, acquisition scripts, tests, and `Input/` complaint forms |

## Build and run from the workspace root

```powershell
cmake -S Tools -B build -G "MinGW Makefiles"
cmake --build build
ctest --test-dir build --output-on-failure
.\build\rag-cli.exe validate-pack --pack .\Policy
.\build\rag-cli.exe run --pack .\Policy
```

The included pack run reads `Tools/Input`, searches `Policy/rules`, assesses each case with the installed local Hermes CLI, and updates `Output/CASE-1`, `Output/CASE-2`, etc. Use `--case 04-credit-score-error.json` for one case or omit it for the whole folder. Use `--offline` for retrieval without Hermes and `--max-documents 3` for a small smoke test. The model does not send or decide cases; a named reviewer approves any action.

## Department packs

A pack is a folder containing `pack.json`, `prompt.txt` and a `rules/` folder. `pack.json` maps an incoming JSON form into the shared case pipeline and supplies report labels and optional field-driven review flags. Dotted field paths are supported; claim and category fields can be text or arrays of text.

Start with `Packs/_template`. Every pack rule must include `review_action` and `required_evidence`, keeping department-specific next steps out of the executable. Run `validate-pack` after every rulebook change. Pack paths and excerpt paths are constrained to their pack folder.

```powershell
Copy-Item .\Packs\_template .\Packs\Procurement -Recurse
.\build\rag-cli.exe validate-pack --pack .\Packs\Procurement
.\build\rag-cli.exe run --pack .\Packs\Procurement --source .\Incoming\Procurement --output .\Output\Procurement
```

`--rules` remains available as an explicit override and running without `--pack` retains the legacy ICO parser for compatibility.

If Hermes returned a response but validation failed, reprocess its saved `hermes-response.validated.json` after updating the validator with `--case FILE --replay-response PATH`. Replay accepts exactly one claim, reruns the same citation and PII checks, and marks the output as `hermes_replay`.

## How the RAG path works

1. Ingest an ICO-style JSON complaint or supported text input. Unknown binary complaint formats are quarantined.
2. Apply field-level and text-level PII filtering. Keep a filtered form and category counts; the token vault is off by default.
3. Split the complaint narrative into source-traceable claims. Use its event date to select rules in force. `Policy/sources` is reference material and is not searched directly.
4. Rank effective clauses using claim terms, issue labels, clause IDs and curated tags. Prepare a finding with retrieval score, candidate rules, missing evidence and review flags. Load an exact legislative excerpt from `Policy/excerpts` when its reviewed rule names an `excerpt_file`; missing excerpts get a visible flag. The `Policy/sources` directory remains reference material and is never used as unreviewed retrieval text.
5. Pass that prepared finding, its flags, the filtered complaint form, up to five candidate summaries and available Article text to Hermes. Accept its assessment only if its chosen clause is among the candidates and its quote matches the claim or the selected checked Article excerpt. A missing or unusual model confidence is withheld. Offline cases show `pending Hermes` with a numeric rule-match score. A bad citation or model failure leaves `insufficient_information` and a review flag. Obvious claims of a proven violation or certainty from an allegation get `MODEL_OVERSTATEMENT_REVIEW`; the report hides that rationale and withholds its confidence while preserving the validated model response for the reviewer.
6. Write one `Output/CASE-N/` folder per complaint. Each folder contains `report.md`, `findings.json` with the action category and concrete `action_detail`, filtered form, PII counts, audit, and Hermes prompts/responses when used. `Output/index.md` lists all case folders. Invalid inputs go under `Output/Quarantine/`.

The confidence number is the model's self-report, not a calibrated probability. The tool does not resolve exemptions, legal hierarchy or all cross-references. Review the linked legislation and the original evidence before a real decision.

## R analytics

Run `.\Tools\run_r_analysis.ps1` after building the harness. It exports a text-free case/finding table and uses local base R to create charts and a Markdown summary in `Output/Analytics`. Reviewer decisions are recorded separately; the current fictional cases have no such labels. See [the R plugin guide](R/README.md) for the data contract, optional model, and R setup.

## Policy acquisition

To save an official source and a SHA-256 provenance record:

```powershell
.\Tools\pull_legislation.ps1 -Url 'https://www.legislation.gov.uk/ukpga/2025/18/pdfs/ukpga_20250018_en.pdf' -Name 'data-use-access-act-2025' -Format pdf
.\Tools\extract_pdf_text.ps1 -Name 'data-use-access-act-2025'
```

Use a new `-Name` for a later version. The script accepts HTTPS sources from legislation.gov.uk, gov.uk and ico.org.uk, and rejects a redirect to HTTP. The text extraction tool needs `pdftotext.exe`. Downloading or extracting a document does not add it to retrieval: review and curate its clauses into `Policy/rules/*.json` (`policy_rules_v1`) or `.txt` first. The included [Policy guide](../Policy/README.md) links the current consolidated UK GDPR and explains the snapshot date.

## Demo complaints

`Tools/seed_demo_cases.ps1` creates 20 fictional ICO-style JSON forms in `Tools/Input` and leaves existing files untouched. `Tools/Input/21-overdue-sar.json` is an additional hand-reviewed deadline fixture. Run the seed script again with `-Force` only if you intend to replace its 20 generated fixtures. The forms are test data, not real complaints.

## Included data-protection input contract

Structured complaints use `ico_personal_data_complaint_v1`, with a case reference, `access` or `processing` type, issue labels, discovery date, respondent name, complainant name, narrative, desired outcome, and supporting evidence array. `Tools/Input` contains examples. The form is our JSON representation of the [ICO's published form](https://ico.org.uk/media/2259547/personal-information-complaints-form-new-final-2307.pdf), not an official machine export.
