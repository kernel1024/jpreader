// ==UserScript==
// @name         Script title
// @match        https://some.site.abc/*
// @match        http://some.site.abc/*
// @run-at       document-idle
// ==/UserScript==
//
// Supported @run-at values:
//   document-start - The script will run before any document begins loading.
//   document-end   - The default if no value is provided. The script will run after the main page is loaded.
//   document-idle  - The script will run after the page and all resources are loaded.
//   context-menu   - The script appears in browser context menu as separate command.
//   translator     - The script will run after "Translate" button clicked, before translation work.

(function() {
    alert("Hello world!");
})();
