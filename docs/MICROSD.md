# microSD model library

Use a FAT32 microSD card. The viewer accesses it in read-only mode. Copy the
contents of `sdcard/` to the card root so these files exist:

```text
/models/metamask.d3m
/models/suzanne.d3m
```

The JSON reports are optional metadata and are ignored by the viewer. The board's
ST-Link USB mass-storage volume is a firmware-programming interface, not the
microSD card. Use a card reader to copy model files, eject the card, then insert
it into the Discovery board.

Tap **SD Models** to mount the card and list up to 32 `.d3m` files. Use Previous /
Next for pages and Refresh after inserting or replacing a card. Tap a row to
validate and load the model. The model is retained in SDRAM, so animation does
not read storage per frame and removing the card does not invalidate it.
The two embedded models are always available through their top-bar buttons.

The current viewer supports 16,384 vertices, a maximum file size of 2 MiB, and
ASCII filenames up to 63 characters including extension. FAT12/16 support from
FatFs is retained, but FAT32 is the documented configuration. exFAT is disabled.
Polling SDMMC reads use an aligned sector buffer; SD DMA/cache interaction is
avoided. File and directory errors appear on screen. Two model arenas preserve
the previous valid mesh if a replacement is corrupt, truncated, or too large.

## Convert another mesh

```powershell
./tools/local/python/python.exe tools/convert_model.py input.stl sdcard/models/model.d3m --color 79b8e8 --crease 60
```

The converter accepts binary and ASCII STL without Blender or third-party Python
packages. `--crease 0` gives hard edges; a larger crease angle smooths adjacent
faces. `--keep-axes` preserves source axes. It always centers and normalizes the
mesh. Optional `--c-dir assets/generated --symbol model` creates a typed embedded
C asset; add that asset to the application registry and build source list to make
it available without the card.
