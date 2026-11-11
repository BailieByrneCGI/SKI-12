# Hermes instructions for this workspace

Work from the workspace root. For complaint triage, validate and run the data-protection pack using `build/rag-cli.exe validate-pack --pack Policy` and `build/rag-cli.exe run --pack Policy --hermes`. It reads `Tools/Input` and the selected pack and writes to `Output`. Consult `Tools/README.md` before changing pack or rule schemas or acquisition tools.

`Policy/sources` contains reference documents and provenance; only reviewed, effective-dated clauses in `Policy/rules` are searchable. Cite the source URL and clause for every supported finding. Treat `engaged` as possible relevance, not proof of a violation. Keep all drafts behind named human review. Do not use or reveal the token vault without specific authorisation.
