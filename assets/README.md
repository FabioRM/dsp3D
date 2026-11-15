# Model assets

`source/metamask.stl` and `source/suzanne.stl` were supplied by the repository owner.
The original bytes are preserved. Their hashes and conversion settings are recorded
in `sdcard/models/*.json`. The original STL headers mention Blender 3.4.0 and 2.79.

No separate mesh license or upstream download URL was supplied. These assets and
their derived representations are not asserted to be covered by the code's BSD
license. MetaMask names and marks belong to their respective owners. Confirm the
original asset terms before redistributing a product containing these models.

Conversion rotates Blender's Z-up coordinates into Y-up, centers the bounding box,
normalizes the largest extent to 2 units, removes degenerate faces, and generates
area-weighted vertex normals with a configurable crease angle. Vertex positions
and normals are deduplicated together to preserve hard edges. The supplied STL
files have no reliable standard material palette: demo colors are assigned by the
conversion command, not recovered from the original MetaMask artwork.

Generated C assets are checked in so a firmware build does not require Python.
Binary models in `sdcard/models` use the same geometry and can be copied directly
to the card's `models` directory. See `docs/MODEL_FORMAT.md`.
