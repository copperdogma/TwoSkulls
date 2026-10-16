# TwoSkulls perfboard planning

Current review draft: [v4 — northern servo, antenna and standoff clearance](draft-v4/README.md), dated2026-10-16.

- [Assembly and clearance drawing](draft-v4/assembly-plan.png)
- [Editable Perfboarder ZIP](draft-v4/twoskulls-v4.perfboarder.zip)
- [Nominal mechanical calculations](draft-v4/mechanical-check.json)
- [Verification and limits](draft-v4/verification.json)

Earlier drafts remain preserved. V4 moves the servo connector north and marks an optional NE standoff/access reserve. It is not a measured cutting template, verified cable interlock, RF test or electrical-load acceptance record.

[Agreed rail, insulation and soldering approach](assembly-technique.md) records the construction discussion.

Run the saved-artifact checks without regenerating designs:

```sh
python3 hardware/perfboard/verify-designs.py
python3 hardware/perfboard/draft-v4/check-mechanics.py
```

The generation scripts are authoring tools: running them replaces draft JSON/ZIPs and can change project IDs or verification files. They are not the check commands.
