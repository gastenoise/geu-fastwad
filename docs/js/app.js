/**
 * Alpine.js Reactive Component for fastwad Documentation & Interactive Playground
 */
document.addEventListener('alpine:init', () => {
  Alpine.data('fastwadApp', () => ({
    // Controls State
    action: 'build',
    inputDir: './textures',
    outputWad: './halflife.wad',
    extractDir: './extracted',
    json: true,
    wad2: false,
    disableDither: false,
    maxSize: 256,
    align: 'center',
    stretch: false,
    overwrite: true,
    quiet: false,
    format: 'png',

    // Dimension Calculator State
    testWidth: 33,
    testHeight: 33,
    testRawName: '{Glass_Texture 01!',
    
    // Tabs
    outputTab: 'cli',

    // Calculated / Generated Outputs
    cliCommand: '',
    sdkSnippet: '',
    configJson: {},
    dimResult: {},
    normalizedName: '',

    init() {
      this.update();
    },

    update() {
      if (window.FastwadEngine) {
        this.cliCommand = window.FastwadEngine.generateCli(this);
        this.sdkSnippet = window.FastwadEngine.generateSdk(this);
        this.dimResult = window.FastwadEngine.calcDimensions(this.testWidth, this.testHeight, this.maxSize, this.stretch);
        this.normalizedName = window.FastwadEngine.normalizeName(this.testRawName);

        this.configJson = {
          format: this.wad2 ? 'WAD2' : 'WAD3',
          json_output: this.json,
          disable_dither: this.disableDither,
          max_size: parseInt(this.maxSize),
          align: this.align,
          stretch: this.stretch,
          allow_overwrite: this.overwrite,
          key_color: { r: 0, g: 0, b: 255 }
        };
      }
    }
  }));
});
