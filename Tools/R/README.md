# R case analytics

This optional local plugin reads a fixed CSV export of the latest `Output/CASE-N/findings.json` files. It uses base R only. It does not read complaint narratives, identities, filtered forms, Hermes prompts, or token mappings. Every row retains `case_id` and `claim_id` so a reviewer can return to the source finding.

From the workspace root, build the C++ harness, install [R for Windows](https://cran.r-project.org/bin/windows/base/) if needed, then run:

```powershell
cmake --build build
.\Tools\run_r_analysis.ps1
```

The wrapper calls `rag-cli export-analytics`, then `Rscript Tools/R/analyse.R`. It writes `Output/Analytics/findings.csv`, `report.md`, count tables, and PNG charts for verdicts, suggested actions, cited clauses, flags, scores, and recorded human decisions. Pass `-RscriptPath 'C:\path\to\Rscript.exe'` if R is not on `PATH`.

To record actual reviewer decisions, copy `reviewer_decisions.example.csv` to `reviewer_decisions.csv` and add one row per reviewed finding. Use `approve`, `amend`, or `reject`, and an ISO date such as `2026-11-04`. The latter file is ignored by Git. A suggested action is never treated as a reviewer decision.

`-FitModel` requests an exploratory logistic model of whether a human approved the suggested action. It requires at least 100 distinct reviewed cases with available model confidence and at least 20 examples of each outcome. It trains on earlier reviews and evaluates on later cases, then reports a Brier score against a constant-rate baseline. It does not change harness findings, legal decisions, or reviewer actions. Do not treat its predictions as case decisions. The current 21 fictional complaints do not provide review labels or enough cases to fit this model.

The charts and tables count **findings**, not proven breaches. Hermes confidence is an uncalibrated model self report; retrieval score is lexical relevance. A real deployment needs a controlled review log, outcome definitions, access controls, and monitoring for drift and differing error rates across case types before any model could inform prioritisation.
