/**
 * fastwad Interactive Documentation Engine
 * Handles dimension calculation, GoldSrc name normalization, and CLI/SDK generation in client-side JS.
 */
window.FastwadEngine = {
  // Snaps dimension to nearest multiple of 16 with micro-stretch
  calcDimensions(origW, origH, maxSize = 256, stretch = false) {
    let w = parseInt(origW) || 64;
    let h = parseInt(origH) || 64;
    let maxS = parseInt(maxSize) || 256;

    if (w <= 0) w = 64;
    if (h <= 0) h = 64;

    if (stretch) {
      return {
        canvasW: maxS,
        canvasH: maxS,
        scaledW: maxS,
        scaledH: maxS,
        mode: 'Stretch to Max',
        padX: 0,
        padY: 0
      };
    }

    // Scale to fit within maxSize
    let scale = Math.min(maxS / w, maxS / h, 1.0);
    let fitW = w * scale;
    let fitH = h * scale;

    let canvasW = Math.min(maxS, Math.max(16, Math.round(fitW / 16.0) * 16));
    let canvasH = Math.min(maxS, Math.max(16, Math.round(fitH / 16.0) * 16));

    // Micro-stretch check (within 2px)
    let microStretched = false;
    let scaledW = fitW;
    let scaledH = fitH;

    if (Math.abs(fitW - canvasW) <= 2.0 && Math.abs(fitH - canvasH) <= 2.0) {
      scaledW = canvasW;
      scaledH = canvasH;
      microStretched = true;
    }

    let padX = Math.max(0, canvasW - Math.round(scaledW));
    let padY = Math.max(0, canvasH - Math.round(scaledH));

    return {
      canvasW,
      canvasH,
      scaledW: Math.round(scaledW),
      scaledH: Math.round(scaledH),
      mode: microStretched ? 'Micro-Stretch (2px Snap)' : (padX > 0 || padY > 0 ? 'Aspect Contain + Pad' : 'Exact Multiple of 16'),
      padX,
      padY
    };
  },

  // Normalizes texture name with GoldSrc rules
  normalizeName(raw) {
    if (!raw || raw.trim() === '') return 'tex';
    let clean = raw.normalize('NFD').replace(/[\u0300-\u036f]/g, ''); // Deaccent
    let out = '';
    let first = true;

    for (let i = 0; i < clean.length; ++i) {
      let c = clean[i];
      if (first && (c === '{' || c === '!' || c === '+' || c === '~')) {
        out += c;
      } else if (/[a-zA-Z0-9_-]/.test(c)) {
        out += c.toLowerCase();
      }
      first = false;
    }

    if (out.length === 0) return 'tex';
    if (out.length > 15) return out.substring(0, 15);
    return out;
  },

  // Generates CLI command string
  generateCli(opts) {
    let cmd = 'fastwad ';
    if (opts.action === 'build') {
      cmd += `build "${opts.inputDir || './textures'}" "${opts.outputWad || './halflife.wad'}"`;
      if (opts.json) cmd += ' --json';
      if (opts.wad2) cmd += ' wad2=true';
      if (opts.disableDither) cmd += ' disable_dither=true';
      if (opts.maxSize !== 256) cmd += ` max_size=${opts.maxSize}`;
      if (opts.stretch) cmd += ' stretch=true';
      if (opts.align !== 'center') cmd += ` align=${opts.align}`;
      if (opts.overwrite) cmd += ' allow_overwrite=true';
      if (opts.quiet) cmd += ' -q';
    } else if (opts.action === 'list') {
      cmd += `list "${opts.outputWad || './halflife.wad'}"`;
      if (opts.json) cmd += ' --json';
    } else if (opts.action === 'extract') {
      cmd += `extract "${opts.outputWad || './halflife.wad'}" "${opts.extractDir || './extracted'}"`;
      if (opts.format === 'bmp') cmd += ' format=bmp';
      if (opts.json) cmd += ' --json';
    }
    return cmd;
  },

  // Generates C++ SDK Snippet
  generateSdk(opts) {
    return `#include <fastwad/fastwad.hpp>
#include <iostream>
#include <vector>

int main() {
    // Single-call directory build with custom options:
    fastwad::TextureOptions opts;
    opts.disable_dither = ${opts.disableDither ? 'true' : 'false'};
    opts.max_size = ${opts.maxSize};
    opts.stretch = ${opts.stretch ? 'true' : 'false'};
    opts.align = "${opts.align}";

    fastwad::BuildResult result = fastwad::BuildWadFromDirectory(
        "${opts.inputDir || './textures'}", 
        "${opts.outputWad || './halflife.wad'}", 
        ${opts.wad2 ? 'fastwad::WadFormat::WAD2' : 'fastwad::WadFormat::WAD3'}, 
        opts, 
        ${opts.overwrite ? 'true' : 'false'}
    );

    std::cout << "Processed: " << result.total_processed << " textures\\n";
    return (int)result.code;
}`;
  }
};
