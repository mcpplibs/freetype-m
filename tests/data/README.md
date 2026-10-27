# Test fonts

| File | What | Source | License |
|---|---|---|---|
| `JetBrainsMono.ttf` | JetBrains Mono (variable: wght 100–800, seven named instances), unmodified | [google/fonts `ofl/jetbrainsmono/JetBrainsMono[wght].ttf`](https://github.com/google/fonts/blob/2e05c1cf00a6e4f40a4b931600a90881c26e15cd/ofl/jetbrainsmono/JetBrainsMono%5Bwght%5D.ttf) at 2e05c1cf | SIL OFL 1.1 ([`OFL-JetBrainsMono.txt`](OFL-JetBrainsMono.txt), same commit) |
| `mwrap-test.ttf`, `mwrap-test.otf` | Small TrueType and CFF fonts with a `kern` table, COLR v0 and v1 over CPAL, an SVG glyph, a format 14 cmap, vertical metrics and an fsType | [`tools/make_test_fonts.py`](../../tools/make_test_fonts.py) (fontTools 4.67.0) | this repository's (Apache-2.0) |
| `mwrap-test.ttf.gz`, `mwrap-test.ttf.Z` | `mwrap-test.ttf`, gzip and compress(1) LZW | the same script | the same |

SHA-256:

```
48715a42ec242c21e9f02692891e147d022299a52e48d5e413e1a942193ffeda  JetBrainsMono.ttf
b2fe5e8987594e9ffd1d2ca52a2f5d73eb8335243893c5d6254b5ad69269591d  OFL-JetBrainsMono.txt
5009c68c97bba0e77c4e69254d450987536741ccc9a490a3e3bb5ec7d00ea57c  mwrap-test.ttf
63dafb198dc3d5d80a1884295dc236055d1657c79fedb6fbea0d46d2bb9c80d7  mwrap-test.otf
1216eafdb6838eb00535d9fe6256fa25e15b4a40c40257e248688d10872101d4  mwrap-test.ttf.gz
65bd44f6bc41d072f41d60189b208f9bca8487a7073c4181f05612bb1291b12f  mwrap-test.ttf.Z
```
