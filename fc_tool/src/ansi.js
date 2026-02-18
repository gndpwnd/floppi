/**
 * ansi.js — ANSI SGR escape code parser
 *
 * Parses ANSI escape sequences (bold, dim, underline, 16 foreground colors)
 * and returns a DocumentFragment with styled spans.
 */

const ANSI_COLORS = {
  30: 'ansi-black',   31: 'ansi-red',     32: 'ansi-green',   33: 'ansi-yellow',
  34: 'ansi-blue',    35: 'ansi-magenta',  36: 'ansi-cyan',    37: 'ansi-white',
  90: 'ansi-bright-black', 91: 'ansi-bright-red', 92: 'ansi-bright-green', 93: 'ansi-bright-yellow',
  94: 'ansi-bright-blue', 95: 'ansi-bright-magenta', 96: 'ansi-bright-cyan', 97: 'ansi-bright-white',
};

const ANSI_REGEX = /\x1b\[([0-9;]*)m/g;

function parseAnsi(text) {
  const fragment = document.createDocumentFragment();
  let lastIndex = 0;
  let bold = false, dim = false, underline = false, color = null;

  ANSI_REGEX.lastIndex = 0;
  let match;

  while ((match = ANSI_REGEX.exec(text)) !== null) {
    if (match.index > lastIndex) {
      const span = document.createElement('span');
      span.textContent = text.slice(lastIndex, match.index);
      if (bold) span.classList.add('ansi-bold');
      if (dim) span.classList.add('ansi-dim');
      if (underline) span.classList.add('ansi-underline');
      if (color) span.classList.add(color);
      fragment.appendChild(span);
    }
    lastIndex = match.index + match[0].length;

    const codes = match[1] ? match[1].split(';').map(Number) : [0];
    for (const code of codes) {
      if (code === 0) { bold = false; dim = false; underline = false; color = null; }
      else if (code === 1) bold = true;
      else if (code === 2) dim = true;
      else if (code === 4) underline = true;
      else if (code === 22) { bold = false; dim = false; }
      else if (code === 24) underline = false;
      else if (code === 39) color = null;
      else if (ANSI_COLORS[code]) color = ANSI_COLORS[code];
    }
  }

  if (lastIndex < text.length) {
    const span = document.createElement('span');
    span.textContent = text.slice(lastIndex);
    if (bold) span.classList.add('ansi-bold');
    if (dim) span.classList.add('ansi-dim');
    if (underline) span.classList.add('ansi-underline');
    if (color) span.classList.add(color);
    fragment.appendChild(span);
  }

  return fragment;
}

export { parseAnsi };
