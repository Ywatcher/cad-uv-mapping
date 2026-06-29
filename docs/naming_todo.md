# Naming TODO

This note tracks the proposed naming direction for the CAD UV mapping APIs.
It is not applied yet.

## Proposed Default

Use `source` and `target` for two-sided mapping APIs.

Rule of thumb:

- function names should use `source` and `target` without `face`
- argument names should use `source_face` and `target_face`
- one-sided evaluation functions should use just `face`
- if `face` is already obvious from the function context, the function name can
  omit it
- when cardinality matters, use `single` and `multiple` in the function name
  instead of relying on a trailing `s`

Examples:

- `map_source_samples_to_target`
- `map_source_face_samples_to_target_faces`
- `map_multiple_source_face_samples_to_multiple_target_faces`
- `evaluate_face_samples`
- `evaluate_single_face_samples`

## Why This Direction

- `source` / `target` keeps the pair balanced and readable.
- `face` only appears where it adds clarity.
- one-sided evaluation is not a correspondence problem, so it should not be
  forced into two-sided terminology.

## Alternative Naming Set

If the project later wants a more projection-oriented naming scheme, use:

- `query` / `reference`

This is a reasonable alternative when the emphasis is geometric lookup or
correspondence rather than data flow.

Tradeoff:

- `query` / `reference` is more semantic for projection/search
- `source` / `target` is easier to scan and more balanced in long signatures

## Rejected Short Forms

Avoid compressed forms such as:

- `qryface`
- `refface`
- `srcface`
- `dstface`

Those are too compressed for a public API and are harder to read in docs and
stack traces.

## Follow-up Work

- Rename mapping APIs if the current naming is finalized.
- Apply the same rule consistently to Python wrappers, native C++ entry points,
  docs, and tests.
- Check whether any helper types should also be renamed to match the final
  source/target convention.
- Prefer explicit cardinality words (`single`, `multiple`) over implicit plural
  forms when the API needs to distinguish one-vs-many behavior.
