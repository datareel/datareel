/* DataReel documentation helpers. Written 10/14/2026.
   - Hides the site header and footer when a page is shown inside the
     Library, Examples or Utilities browser frame.
   - Browser pages: sidebar filter, active link, and #/page.htm deep
     links that can be bookmarked and shared. */
(function () {
  "use strict";
  var framed = false;
  try { framed = window.self !== window.top; } catch (e) { framed = true; }
  if (framed) document.documentElement.classList.add("dr-framed");

  function setupBrowser() {
    var frame = document.querySelector(".dr-frame");
    var sidebar = document.querySelector(".dr-sidebar");
    if (!frame || !sidebar) return;
    var links = Array.prototype.slice.call(sidebar.querySelectorAll(".dr-group a"));
    var base = location.href.replace(/[#?].*$/, "").replace(/[^\/]*$/, "");

    function markActive(path) {
      links.forEach(function (a) {
        var on = a.getAttribute("href") === path;
        a.classList.toggle("active", on);
        if (on) {
          var group = a.closest("details");
          if (group) group.open = true;
        }
      });
    }

    function fromHash() {
      var h = decodeURIComponent(location.hash.replace(/^#\/?/, ""));
      // Only pages inside this documentation folder
      if (h && /^[A-Za-z0-9_\-\/.]+(#[A-Za-z0-9_\-.]*)?$/.test(h) && h.indexOf("..") < 0) {
        frame.setAttribute("src", h);
        markActive(h.split("#")[0]);
        return true;
      }
      return false;
    }

    links.forEach(function (a) {
      a.addEventListener("click", function () {
        var href = a.getAttribute("href");
        markActive(href.split("#")[0]);
        if (history.replaceState) history.replaceState(null, "", "#/" + href);
      });
    });

    frame.addEventListener("load", function () {
      try {
        var p = frame.contentWindow.location.href;
        if (p.indexOf(base) === 0) {
          var rel = p.substring(base.length);
          markActive(rel.split("#")[0]);
          var doc = frame.contentDocument;
          if (doc && doc.title) document.title = doc.title + " - DataReel";
        }
      } catch (e) { /* page from another origin */ }
    });

    window.addEventListener("hashchange", fromHash);
    if (!fromHash()) markActive(frame.getAttribute("src"));

    var search = sidebar.querySelector(".dr-search");
    if (search) {
      search.addEventListener("input", function () {
        var q = search.value.trim().toLowerCase();
        sidebar.querySelectorAll(".dr-group").forEach(function (g) {
          var any = false;
          g.querySelectorAll("a").forEach(function (a) {
            var hit = !q || a.textContent.toLowerCase().indexOf(q) >= 0;
            a.style.display = hit ? "" : "none";
            if (hit) any = true;
          });
          g.style.display = any ? "" : "none";
          if (q && any) g.open = true;
        });
      });
    }
  }

  if (document.readyState === "loading") document.addEventListener("DOMContentLoaded", setupBrowser);
  else setupBrowser();
})();
