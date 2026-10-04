# Release preparation checklist

This folder is a source-and-content candidate, not a published or Fab-approved release.

- [x] Repository owner selected MIT (Copyright 2026 Tofu); LICENSE files are included at repository and standalone plugin roots.
- [ ] Add publisher identity, documentation/support URLs and listing metadata to the descriptor.
- [ ] Supply an original plugin icon and listing media; do not use the learning project's character or tutorial images without permission.
- [ ] Verify builds/cooks on every advertised target. Initial scope is UE 5.8 / Win64 only, not a claim about other versions or platforms.
- [ ] Recheck current Fab requirements and complete its review. Local assembly/build validation is not Fab acceptance.
- [ ] Confirm rights to all source/content. This candidate excludes character models, textures and tutorial graphs.
- [ ] Review the final ZIP manifest: exactly one plugin; no project, experiments, test maps, binaries, caches or private paths.

Official references checked during preparation on 2026-10-02:

- [Epic: code-plugin file structure](https://dev.epicgames.com/documentation/en-us/fab/asset-file-format-and-structure-requirements-in-fab)
- [Epic: Unreal Engine plugins and content](https://dev.epicgames.com/documentation/en-us/unreal-engine/plugins-in-unreal-engine)

The plugin preserves the existing runtime system. Its isolated release tests use an Engine sphere and the three bundled assets; the original character-dependent lab builder and tests are not distributed.
