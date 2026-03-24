# Security policy

## Reporting

If you find a security issue, do not open a public issue with the secret or exploit details.

Open a private report through GitHub Security Advisories if enabled for the repository, or contact the maintainer directly and include:

- a short description of the issue
- the affected file or feature
- steps to reproduce
- impact and suggested mitigation

## Secrets

This repository does not track runtime secrets.

- Use `secrets.h` locally and keep it out of Git.
- Do not commit API keys, Wi-Fi credentials, tokens, or private workflow exports.
- If a secret is exposed, rotate it immediately even if the commit history is later rewritten.

## Supported versions

There is currently a single active line of development on `main`.
