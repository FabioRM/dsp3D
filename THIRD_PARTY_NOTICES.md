# Third-party notices

The dsp3D code is BSD-3-Clause; third-party software retains its own terms.

- `third_party/STM32CubeF7` is a selected, unmodified source subset of
  STM32Cube_FW_F7_V1.17.4 supplied on the development machine. CMSIS Core/DSP and
  the STM32 device layer carry Arm/ST notices; HAL, board/component drivers,
  fonts, and FatFs carry their original notices. See each component's `LICENSE`
  file, source headers, and `third_party/STM32CubeF7/Package_license.md` for the
  package bill of materials and full terms.
- `legacy/` preserves the original 2016 dsp3D implementation and its attribution
  to David Rousset's software-rendering tutorial series at https://www.davrous.com.
- The two supplied model files and their conversions have separate provenance;
  see `assets/README.md`. No asset license is invented or implied by the code license.
- Portable development executables in ignored `tools/local/` retain their
  original license files. They are local workstation dependencies, not part of
  the source-code BSD grant or Git history.
