"""Prepare host-side SD file fixtures; no card access or host dependency at runtime."""
from pathlib import Path
import shutil
root = Path(__file__).resolve().parents[1]
target = root / 'build/native/sd-fixture'
shutil.copytree(root / 'sdcard', target, dirs_exist_ok=True)
data = bytearray((target / 'models/suzanne-v2.d3m').read_bytes())
data[-1] ^= 1
(target / 'models/bad.d3m').write_bytes(data)
folder = target / 'models/examples'
data = bytearray((folder / 'ribbon.d3s').read_bytes())
data[-1] ^= 1
(folder / 'badscene.d3s').write_bytes(data)
(folder / 'badscene.catalog').write_text('ribbon.d3m\n')
