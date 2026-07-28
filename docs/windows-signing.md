# Windows release signing

RVRSE release tags and manually approved release candidates use Microsoft Artifact Signing with GitHub OIDC. No certificate private key or Azure client secret is stored in GitHub.

## Azure setup

1. Use a paid Azure subscription whose billing account type is **Organization** and whose legal name and address exactly match the business registry.
2. Register the `Microsoft.CodeSigning` resource provider.
3. Create a Basic Artifact Signing account. The current account is `rvrsesigning-01` in North Europe.
4. Assign the human validator **Artifact Signing Identity Verifier** on the Artifact Signing account, then complete a Public Trust organization identity validation.
5. After approval, create a Public Trust certificate profile named `rvrse-public-release`.
6. Create a Microsoft Entra application named `github-rverse-signing`.
7. Add a federated credential for:
   - Organization: `SamuFL`
   - Repository: `rverse`
   - Entity type: `Environment`
   - Environment: `production-signing`
8. Assign the application's service principal **Artifact Signing Certificate Profile Signer** at the certificate-profile scope:

   ```text
   /subscriptions/<subscription-id>/resourceGroups/rvrse-signing-prod/providers/Microsoft.CodeSigning/codeSigningAccounts/rvrsesigning-01/certificateProfiles/rvrse-public-release
   ```

The public certificate and Windows UAC dialog show the registered legal organization name. SamuFL remains the product-facing brand in installer metadata.

## GitHub environment

Create a `production-signing` environment under **Settings → Environments**:

1. Add the repository owner as a required reviewer.
2. Disable administrator bypass.
3. Restrict deployment branches/tags to release branches and version tags as appropriate.
4. Add these environment variables:

| Variable | Value |
|---|---|
| `AZURE_CLIENT_ID` | Entra application client ID |
| `AZURE_TENANT_ID` | Microsoft Entra tenant ID |
| `AZURE_SUBSCRIPTION_ID` | Paid Azure subscription ID |
| `AZURE_ARTIFACT_SIGNING_ENDPOINT` | `https://neu.codesigning.azure.net/` |
| `AZURE_ARTIFACT_SIGNING_ACCOUNT` | `rvrsesigning-01` |
| `AZURE_ARTIFACT_SIGNING_PROFILE` | `rvrse-public-release` |

These identifiers are configuration, not credentials. OIDC exchanges the GitHub environment identity for a short-lived Azure token.

## Release flow

The Windows release job:

1. Builds and tests the Release binaries.
2. Signs the VST3 binary, CLAP plugin, and standalone executable with SHA-256 and an RFC 3161 timestamp.
3. Verifies each embedded signature with SignTool.
4. Builds an Inno Setup installer and a portable ZIP containing the same signed binaries.
5. Signs and verifies the installer.
6. Silently installs and uninstalls it on the ephemeral Windows runner and checks for orphaned files.

Tag pushes publish the installer and ZIP. Manual workflow dispatches produce approved release-candidate artifacts without creating a public GitHub Release. Ordinary CI and development builds remain unsigned.

## Local packaging

On Windows with Visual Studio 2022 and Inno Setup 6.3 or newer:

```powershell
cmake --preset windows-vs2022
cmake --build build/windows-vs2022 --config Release

./scripts/sign-and-package-win.ps1 `
  -Action Package `
  -BuildDirectory build/windows-vs2022/out `
  -OutputDirectory build/windows-vs2022/dist `
  -Version 1.1.0
```

Add `-RequireSignedInputs` when packaging production-signed binaries. After the installer is signed, verify signatures and the install lifecycle:

```powershell
./scripts/sign-and-package-win.ps1 `
  -Action Verify `
  -BuildDirectory build/windows-vs2022/out `
  -OutputDirectory build/windows-vs2022/dist `
  -Version 1.1.0 `
  -RequireSignedInputs `
  -TestInstall
```
