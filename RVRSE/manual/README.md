# RVRSE user manual

`RVRSE manual.txt` is the editable source and `RVRSE manual.pdf` is the
distribution copy bundled with Windows release ZIPs.

On macOS, regenerate the PDF after editing the source:

```bash
/usr/sbin/cupsfilter -m application/pdf "RVRSE/manual/RVRSE manual.txt" \
  > "RVRSE/manual/RVRSE manual.pdf"
```
