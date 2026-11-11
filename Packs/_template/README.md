# Department pack template

Copy this entire folder, rename it for the department, and edit `pack.json`, `prompt.txt` and `rules/rules.json`.

Validate and smoke-test the copy from the workspace root:

```powershell
.\build\rag-cli.exe validate-pack --pack .\Packs\YourDepartment
.\build\rag-cli.exe run --pack .\Packs\YourDepartment --source .\Packs\YourDepartment\examples --output .\Output\YourDepartment --offline
```

`pack.json` maps the department's JSON fields into the common case pipeline. Dotted paths such as `employee.team` are supported. Claim and category fields may contain either text or arrays of text.

Every rule requires `id`, `text`, `review_action` and `required_evidence`. Statutory rules additionally require `source_url`; an optional reviewed `excerpt_file` must point to a text file inside the pack.
