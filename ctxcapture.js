el = document.elementFromPoint(window.mouseXPos, window.mouseYPos);

tagName = el.tagName.toUpperCase();
href = "-";
srcc = "-";
title = "-";

if (el.hasAttribute('TITLE')) {
    title = el.getAttribute('TITLE');
}

if (tagName == 'A') {
    href = el.getAttribute('HREF');
} else if (tagName == 'IMG') {
    srcc = el.getAttribute('SRC');
}

window.mouseXPos + '#ZVSP#' +
window.mouseYPos + '#ZVSP#' +
tagName + '#ZVSP#' +
href + '#ZVSP#' +
srcc + '#ZVSP#' +
title
