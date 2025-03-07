const fs = require('fs');
const zlib = require('zlib');

const htmlPath = 'index.html';
const scriptPath = 'bundle.js';
const headerPath = 'dash.h';
const codePath = 'dash.cpp';

const html = fs.readFileSync(htmlPath, 'utf8');
const script = fs.readFileSync(scriptPath, 'utf8');
const combined = html.replace('%%%SCRIPT%%%', () => script);
const combinedCompressed = zlib.gzipSync(Buffer.from(combined, 'utf8'));

const length = combinedCompressed.length;
const bytesNum = Array.from(combinedCompressed).join(', ');

const headerContent = `#include <Arduino.h>\n\nextern const uint8_t DASH[${length}];`;
fs.writeFileSync(headerPath, headerContent, 'utf8');

const codeContent = `#include \"dash.h\"\n\nconst uint8_t DASH[${length}] PROGMEM = { ${bytesNum} };`;
fs.writeFileSync(codePath, codeContent, 'utf8');
