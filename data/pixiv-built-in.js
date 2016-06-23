// ==UserScript==
// @name        pixiv-burst-load
// @namespace   pixivburst
// @description Pixiv burst load for all images in manga mode
// @include     http://www.pixiv.net/member_illust.php?*mode=manga*
// ==/UserScript==

var imgs = document.getElementsByClassName("image ui-scroll-view");
var i;
for (i = 0; i < imgs.length; i++) {
  var src = imgs[i].getAttribute("src");
  var data = imgs[i].getAttribute("data-src");
  if (src.indexOf("transparent")>-1)
    imgs[i].setAttribute("src",data);
}
