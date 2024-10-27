# TODO

- [ ] Propagate all decoder/encoder/mux/write failures to the CLI exit status.
- [ ] Handle pipe/fork/exec failures consistently and avoid fixed shared temp names.
- [ ] Cover decoder draining and variable-frame-rate input with regression fixtures.
- [ ] Support videos without audio analysis input.
- [ ] Budget RAM for long/high-resolution renders.
- [ ] Add live video preview; move AUTO PARTS work off the GTK event loop.
- [ ] Load/save GUI settings and presets.
- [ ] Add tests for FFT curves, DCT round trips, arrangement constraints and each effect.
- [ ] Either implement or explicitly retire unused codec-level effect stubs.
- [ ] Support full Annex B parsing before claiming standalone H.264 stream editing.
- [ ] Define sidecar hold/drop/freeze semantics and test actual beat-to-frame timing.

Current behavior and limitations: [KNOWN_ISSUES.md](KNOWN_ISSUES.md).
