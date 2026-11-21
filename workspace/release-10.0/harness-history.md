# Harness notes

- Initial identity UI check rejected the micro-size white TRAMONTANA caption on the new photographic background at the generic 0.88 match threshold. Actual framebuffer correlation was 0.817 at the expected caption, with the compiled label and version already verified. Only this photo-overlaid micro-caption now uses face 6 / 0.80. All other identity assertions remain unchanged. The failed receipt is not counted as PASS; the full identity test is rerun.
- The optional OCR diagnostic fallback was not provisioned in the recreated tool root. It masked that assertion with missing-tesseract; framebuffer pixel matching itself does not use OCR.
- Original production sources were frozen after the Galleria thirteenth-entry bounds fix and all residual active release-branding updates. Final tests bind to that ISO SHA; pre-freeze attempts do not count.
