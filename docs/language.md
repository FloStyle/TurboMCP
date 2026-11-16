# English-language policy

[Documentation](README.md)

Use English for repository-authored documentation, comments, tool descriptions, tasks, filenames, issues, pull requests and commit messages. Keep a single English documentation tree. Preserve technical identifiers and required legal notices; ask the owner to resolve a conflict instead of silently removing attribution.

## Automated check

Run `python Tools/check-language.py`. The offline validation gate runs it as well. It checks all tracked and non-ignored candidate files, including historical task records in the current tree.

The check rejects non-ASCII letters, invisible formatting controls, non-ASCII filenames and invalid UTF-8 text. It permits English typographic punctuation and mathematical symbols. Negative test samples use escaped code points so they do not introduce foreign-language prose into source files.

This is a deterministic character and encoding check, not a language classifier. French, German or other prose written with ASCII characters still requires review. Reviewers must read new prose for English and clarity.

## Binary and history coverage

Recognized binary assets are listed in the check output rather than silently treated as validated text. Unreal assets can contain serialized display names, comments and text; images can contain visible writing. Their contents require editor export or visual review before an English-only release claim. Do not rewrite binary assets using byte replacement.

Git history, ignored recovery archives and GitHub-hosted issues or release bodies are outside this working-tree check. Follow the [source publication policy](public-preparation.md) for retained-history review. The working-tree result applies to the checked snapshot. Keep the unsanitized recovery history private.
