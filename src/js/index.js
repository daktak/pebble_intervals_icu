var Clay = require("@rebble/clay");
var clayConfig = require("./config.json");
var clay = new Clay(clayConfig);
var base64 = require("base-64");

var STORE_KEY = "icu_api_key";
var STORE_ID = "icu_athlete_id";

var API_KEY = localStorage.getItem(STORE_KEY) || "";
var ATHLETE_ID = localStorage.getItem(STORE_ID) || "0";

function pad(n) {
  return (n < 10 ? "0" : "") + n;
}

function fmt(d) {
  return d.getFullYear() + "-" + pad(d.getMonth() + 1) + "-" + pad(d.getDate());
}

function daysAgo(n) {
  var d = new Date();
  d.setDate(d.getDate() - n);
  return fmt(d);
}

function getJSON(url, cb) {
  var xhr = new XMLHttpRequest();
  xhr.open("GET", url, true);
  xhr.setRequestHeader("Authorization", "Basic " + base64.encode("API_KEY:" + API_KEY));
  console.log("FETCH url=" + url.replace(API_KEY, "***"));
  console.log("FETCH auth=" + (API_KEY ? "set" : "MISSING"));
  xhr.onload = function () {
    console.log("FETCH status=" + xhr.status + " len=" + (xhr.responseText ? xhr.responseText.length : 0));
    if (xhr.status >= 200 && xhr.status < 300) {
      try {
        cb(null, JSON.parse(xhr.responseText));
      } catch (e) {
        console.log("FETCH parse error: " + e.message);
        cb(new Error("bad response"));
      }
    } else {
      console.log("FETCH http error statusText=" + xhr.statusText);
      cb(new Error("HTTP " + xhr.status));
    }
  };
  xhr.onerror = function () {
    console.log("FETCH network error");
    cb(new Error("network error"));
  };
  xhr.send();
}

function fetchWeek() {
  if (!API_KEY) {
    Pebble.sendAppMessage({ ERR: "Set your API key in Settings" });
    return;
  }
  var url =
    "https://intervals.icu/api/v1/athlete/" +
    (ATHLETE_ID || "0") +
    "/activities?oldest=" +
    daysAgo(6) +
    "&newest=" +
    daysAgo(0) +
    "&fields=id,start_date_local,type,name,icu_training_load";
  getJSON(url, function (err, data) {
    if (err) {
      console.log("WEEK err=" + err.message);
      Pebble.sendAppMessage({ ERR: "Activities failed: " + err.message }, function (e) {
        console.log("WEEK sendAppMessage result=" + (e && e.error ? "err:" + e.error : "ok"));
      });
      return;
    }
    console.log("WEEK count=" + data.length);
    if (data.length > 0) console.log("WEEK sampleKeys=" + JSON.stringify(Object.keys(data[0])));
    var lines = [];
    var i;
    for (i = 0; i < data.length; i++) {
      var a = data[i];
      var date = (a.start_date_local || "").substring(0, 10);
      var type = a.type || "";
      var name = (a.name || "").replace(/[\n|;]/g, " ");
      var load = a.icu_training_load != null ? a.icu_training_load : 0;
      lines.push(date + "|" + type + "|" + name + "|" + load);
    }
    if (lines.length > 0) console.log("WEEK firstLine=" + lines[0]);
    var payload = lines.join("\n");
    console.log("WEEK sending ACTIVITIES len=" + payload.length);
    Pebble.sendAppMessage({ ACTIVITIES: payload }, function (e) {
      console.log("WEEK sendAppMessage result=" + (e && e.error ? "err:" + e.error : "ok"));
    });
  });
}

function fetchLoad() {
  if (!API_KEY) {
    Pebble.sendAppMessage({ ERR: "Set your API key in Settings" });
    return;
  }
  var url =
    "https://intervals.icu/api/v1/athlete/" +
    (ATHLETE_ID || "0") +
    "/wellness?oldest=" +
    daysAgo(27) +
    "&newest=" +
    daysAgo(0);
  getJSON(url, function (err, data) {
    if (err) {
      console.log("LOAD err=" + err.message);
      Pebble.sendAppMessage({ ERR: "Load failed: " + err.message }, function (e) {
        console.log("LOAD sendAppMessage result=" + (e && e.error ? "err:" + e.error : "ok"));
      });
      return;
    }
    console.log("LOAD count=" + data.length);
    if (data.length > 0) console.log("LOAD sampleKeys=" + JSON.stringify(Object.keys(data[0])));
    var ctl = [];
    var atl = [];
    var last = null;
    var i;
    for (i = 0; i < data.length; i++) {
      var w = data[i];
      if (typeof w.ctl === "number") ctl.push(Math.round(w.ctl));
      if (typeof w.atl === "number") atl.push(Math.round(w.atl));
      last = w;
    }
    var c = last && typeof last.ctl === "number" ? Math.round(last.ctl) : 0;
    var a = last && typeof last.atl === "number" ? Math.round(last.atl) : 0;
    var t = last && typeof last.tsb === "number" ? Math.round(last.tsb) : c - a;
    var series = "ctl:" + ctl.join(",") + ";atl:" + atl.join(",");
    console.log("LOAD ctl=" + c + " atl=" + a + " tsb=" + t + " seriesLen=" + series.length);
    Pebble.sendAppMessage({ TL_CTL: c, TL_ATL: a, TL_TSB: t, TL_SERIES: series }, function (e) {
      console.log("LOAD sendAppMessage result=" + (e && e.error ? "err:" + e.error : "ok"));
    });
  });
}

Pebble.addEventListener("appmessage", function (e) {
  var p = e.payload;
  if (typeof p.API_KEY !== "undefined") {
    API_KEY = p.API_KEY;
    localStorage.setItem(STORE_KEY, API_KEY);
  }
  if (typeof p.ATHLETE_ID !== "undefined") {
    ATHLETE_ID = p.ATHLETE_ID;
    localStorage.setItem(STORE_ID, ATHLETE_ID);
  }
  if (typeof p.CMD === "undefined") return;
  if (p.CMD === 1) {
    Pebble.openURL(clay.generateUrl());
  } else if (p.CMD === 2) {
    fetchWeek();
  } else if (p.CMD === 3) {
    fetchLoad();
  }
});

Pebble.addEventListener("ready", function () {
  console.log("Intervals.icu JS ready");
});
