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

## OTA Updates

Over-The-Air (OTA) firmware updates are implemented with the following minimum security policy:
- **Transport**: All OTA metadata (`manifest.json`) and binary payloads (`.bin`) MUST be fetched over HTTPS.
- **Verification**: The device MUST verify the expected `board` and `channel` before accepting an update.
- **Downgrade protection**: The device MUST reject any update where the remote version is older than or equal to the currently installed version.
- **Integrity**: Full SHA256 validation post-flash is not currently supported by `HTTPUpdate`. The manifest securely transports the SHA256 checksum, which is logged for diagnostic purposes pending future implementation of fully validated flash writing.
- **State**: The device MUST NOT start an update if the battery level is below the minimum required by the manifest, or if the binary size exceeds the OTA partition size.
