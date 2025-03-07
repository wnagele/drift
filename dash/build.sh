#!/bin/sh
set -e
npm install
npm run webpack
node wrap.js
