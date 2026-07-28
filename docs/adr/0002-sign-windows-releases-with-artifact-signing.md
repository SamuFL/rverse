# Sign Windows releases with Microsoft Artifact Signing

RVRSE uses a Microsoft Artifact Signing Public Trust organization profile rather than exporting a traditional certificate private key. GitHub Actions authenticates with short-lived OIDC credentials scoped to a reviewer-protected `production-signing` environment, signs only version releases and approved release candidates, and leaves ordinary development builds unsigned.

The existing Inno Setup path remains the Windows installer format because it provides the required per-machine plugin installation and uninstall behavior without the enterprise deployment complexity of WiX/MSI. Releases publish both the signed installer and a ZIP containing the same signed binaries; the public signature shows the registered legal organization while product-facing metadata continues to use the SamuFL brand.
