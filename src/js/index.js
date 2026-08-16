var Clay = require("@rebble/clay");
var clayConfig = require("./config.json");
var clay = new Clay(clayConfig);
var base64 = require("base-64");

var STORE_KEY = "icu_api_key";
var STORE_ID = "icu_athlete_id";

var API_KEY = localStorage.getItem(STORE_KEY) || "";
var ATHLETE_ID = localStorage.getItem(STORE_ID) || "0";
var weekActivities = [];

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

function claySettings() {
  try {
    var raw = localStorage.getItem("clay-settings");
    if (raw) return JSON.parse(raw);
  } catch (e) {}
  return {};
}

function effApiKey() {
  var s = claySettings();
  return s.API_KEY && s.API_KEY !== "" ? s.API_KEY : API_KEY;
}

function effAthleteId() {
  var s = claySettings();
  return s.ATHLETE_ID && s.ATHLETE_ID !== "" ? s.ATHLETE_ID : ATHLETE_ID;
}

function units() {
  var u = localStorage.getItem("icu_units") || "metric";
  return u === "imperial" ? "imperial" : "metric";
}

function effUnits() {
  var s = claySettings();
  return s.UNITS && s.UNITS !== "" ? s.UNITS : units();
}

function fmtTime(sec) {
  sec = sec || 0;
  var h = Math.floor(sec / 3600);
  var m = Math.floor((sec % 3600) / 60);
  var s = sec % 60;
  if (h > 0) return h + ":" + pad(m) + ":" + pad(s);
  return m + ":" + pad(s);
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
  API_KEY = effApiKey();
  ATHLETE_ID = effAthleteId();
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
    "&fields=id,start_date_local,type,name,icu_training_load,distance,moving_time,total_elevation_gain,average_speed,icu_intensity,average_heartrate,max_heartrate,icu_weighted_avg_watts,icu_average_watts,icu_joules";
  getJSON(url, function (err, data) {
    if (err) {
      console.log("WEEK err=" + err.message);
      Pebble.sendAppMessage({ ERR: "Activities failed: " + err.message }, function (e) {
        console.log("WEEK " + (e && e.error ? "err:" + e.error : "ok"));
      });
      return;
    }
    console.log("WEEK count=" + data.length);
    if (data.length > 0) console.log("WEEK sampleKeys=" + JSON.stringify(Object.keys(data[0])));
    weekActivities = data;
    var dows = ["Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"];
    var lines = [];
    var i;
    for (i = 0; i < data.length; i++) {
      var a = data[i];
      var date = (a.start_date_local || "").substring(0, 10);
      var type = a.type || "";
      var name = (a.name || "").replace(/[\n|;]/g, " ");
      var load = a.icu_training_load != null ? a.icu_training_load : 0;
      var d = new Date(a.start_date_local);
      var dow = isNaN(d.getDay()) ? "" : dows[d.getDay()];
      lines.push(date + "|" + type + "|" + name + "|" + load + "|" + dow);
    }
    if (lines.length > 0) console.log("WEEK firstLine=" + lines[0]);
    var payload = lines.join("\n");
    console.log("WEEK sending ACTIVITIES len=" + payload.length);
    Pebble.sendAppMessage({ ACTIVITIES: payload }, function (e) {
      console.log("WEEK sendAppMessage result=" + (e && e.error ? "err:" + e.error : "ok"));
    });
  });
}

function sendActivityDetail(idx) {
  if (!weekActivities || !weekActivities[idx]) {
    Pebble.sendAppMessage({ ACTIVITY_DETAIL: "Error|no data" });
    return;
  }
  var a = weekActivities[idx];
  var u = effUnits();
  var dist = a.distance || 0;
  var distU = u === "imperial" ? "mi" : "km";
  dist = u === "imperial" ? dist / 1609.34 : dist / 1000;
  var elv = a.total_elevation_gain || 0;
  var elvU = u === "imperial" ? "ft" : "m";
  elv = u === "imperial" ? elv * 3.28084 : elv;
  var spd = a.average_speed || 0;
  spd = u === "imperial" ? spd * 2.23694 : spd * 3.6;
  var rows = [
    "Elevation|" + Math.round(elv) + " " + elvU,
    "Distance|" + dist.toFixed(1) + " " + distU,
    "Time|" + fmtTime(a.moving_time),
    "Avg Speed|" + spd.toFixed(1) + (u === "imperial" ? " mph" : " km/h"),
    "Intensity|" + Math.round((a.icu_intensity || 0) * 100) + " %",
    "Load|" + Math.round(a.icu_training_load || 0),
    "Avg HR|" + (a.average_heartrate ? a.average_heartrate + " bpm" : "-"),
    "Max HR|" + (a.max_heartrate ? a.max_heartrate + " bpm" : "-"),
    "Norm Power|" + (a.icu_weighted_avg_watts ? a.icu_weighted_avg_watts + " W" : "-"),
    "Avg Power|" + (a.icu_average_watts ? a.icu_average_watts + " W" : "-"),
    "Work|" + (a.icu_joules ? (a.icu_joules / 1000).toFixed(0) + " kJ" : "-")
  ];
  var payload = rows.join("\n");
  console.log("DETAIL sending idx=" + idx + " rows=" + rows.length);
  Pebble.sendAppMessage({ ACTIVITY_DETAIL: payload });
}

function fetchLoad() {
  API_KEY = effApiKey();
  ATHLETE_ID = effAthleteId();
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
    var tsb = [];
    var last = null;
    var i;
    for (i = 0; i < data.length; i++) {
      var w = data[i];
      if (typeof w.ctl === "number") ctl.push(Math.round(w.ctl));
      if (typeof w.atl === "number") atl.push(Math.round(w.atl));
      var tv = (typeof w.tsb === "number") ? w.tsb : (ctl[ctl.length - 1] - atl[atl.length - 1]);
      tsb.push(Math.round(tv));
      last = w;
    }
    var c = last && typeof last.ctl === "number" ? Math.round(last.ctl) : 0;
    var a = last && typeof last.atl === "number" ? Math.round(last.atl) : 0;
    var t = last && typeof last.tsb === "number" ? Math.round(last.tsb) : c - a;
    var series = "ctl:" + ctl.join(",") + ";atl:" + atl.join(",") + ";tsb:" + tsb.join(",");
    console.log("LOAD ctl=" + c + " atl=" + a + " tsb=" + t + " seriesLen=" + series.length);
    Pebble.sendAppMessage({ TL_CTL: c, TL_ATL: a, TL_TSB: t, TL_SERIES: series }, function (e) {
      console.log("LOAD sendAppMessage result=" + (e && e.error ? "err:" + e.error : "ok"));
    });
  });
}

function fetchStats() {
  API_KEY = effApiKey();
  ATHLETE_ID = effAthleteId();
  if (!API_KEY) {
    Pebble.sendAppMessage({ ERR: "Set your API key in Settings" });
    return;
  }
  var s = claySettings();
  var aid = s.ATHLETE_ID && s.ATHLETE_ID !== "" ? s.ATHLETE_ID : ATHLETE_ID;
  var src = s.ATHLETE_ID && s.ATHLETE_ID !== "" ? "clay" : "fallback";
  console.log("STATS aid=" + aid + " src=" + src);
  var base = "https://intervals.icu/api/v1/athlete/" + (aid || "0");
  var actUrl =
    base +
    "/activities?oldest=" +
    daysAgo(6) +
    "&newest=" +
    daysAgo(0) +
    "&fields=id,start_date_local,type,name,icu_training_load,distance,moving_time,elapsed_time,total_elevation_gain,calories";
  var wellUrl =
    base + "/wellness?oldest=" + daysAgo(6) + "&newest=" + daysAgo(0);
  getJSON(actUrl, function (err, acts) {
    if (err) {
      console.log("STATS acts err=" + err.message);
      Pebble.sendAppMessage({ ERR: "Stats failed: " + err.message });
      return;
    }
    console.log("STATS acts count=" + (acts ? acts.length : 0));
    if (acts && acts.length > 0) console.log("STATS acts sampleKeys=" + JSON.stringify(Object.keys(acts[0])));
    var tt = 0;
    var td = 0;
    var ld = 0;
    var kc = 0;
    var el = 0;
    var i;
    for (i = 0; i < (acts ? acts.length : 0); i++) {
      var a = acts[i];
      var t = a.moving_time != null ? a.moving_time : (a.elapsed_time != null ? a.elapsed_time : 0);
      tt += t;
      td += a.distance || 0;
      ld += a.icu_training_load || 0;
      kc += a.calories || 0;
      el += a.total_elevation_gain || 0;
    }
    getJSON(wellUrl, function (err2, well) {
      if (err2) {
        console.log("STATS wellness err=" + err2.message);
        well = [];
      }
      var ctl = 0;
      var atl = 0;
      var tsb = 0;
      var ramp = 0;
      if (well && well.length > 0) {
        var first = well[0];
        var last = well[well.length - 1];
        ctl = Math.round(last.ctl || 0);
        atl = Math.round(last.atl || 0);
        tsb = Math.round(last.tsb != null ? last.tsb : ctl - atl);
        ramp = ctl - Math.round(first.ctl || 0);
      }
      var u = effUnits();
      var dist = 0;
      var distU = "km";
      var elv = 0;
      var elvU = "m";
      if (u === "imperial") {
        dist = td / 1609.34;
        distU = "mi";
        elv = el * 3.28084;
        elvU = "ft";
      } else {
        dist = td / 1000;
        distU = "km";
        elv = el;
        elvU = "m";
      }
      var lines = [];
      lines.push("TT " + (tt / 3600).toFixed(1) + "h  TD " + dist.toFixed(1) + distU);
      lines.push("LD " + Math.round(ld) + "  KC " + Math.round(kc));
      lines.push("EL " + Math.round(elv) + elvU + " F " + ctl + "/" + atl);
      lines.push("FM " + (tsb >= 0 ? "+" : "") + tsb + " RM " + (ramp >= 0 ? "+" : "") + ramp);
      var payload = lines.join("\n");
      console.log("STATS sending payload=" + payload);
      Pebble.sendAppMessage({ STATS: payload });
    });
  });
}

Pebble.addEventListener("appmessage", function (e) {
  var p = e.payload;
  console.log("appmessage keys=" + JSON.stringify(Object.keys(p)));
  if (typeof p.API_KEY !== "undefined") {
    API_KEY = p.API_KEY;
    localStorage.setItem(STORE_KEY, API_KEY);
  }
  if (typeof p.ATHLETE_ID !== "undefined") {
    ATHLETE_ID = p.ATHLETE_ID;
    localStorage.setItem(STORE_ID, ATHLETE_ID);
  }
  if (typeof p.UNITS !== "undefined") {
    localStorage.setItem("icu_units", p.UNITS);
  }
  if (typeof p.CMD === "undefined") {
    if (API_KEY) fetchStats();
    return;
  }
  if (p.CMD === 1) {
    Pebble.openURL(clay.generateUrl());
  } else if (p.CMD === 2) {
    fetchWeek();
  } else if (p.CMD === 3) {
    fetchLoad();
  } else if (p.CMD === 4) {
    console.log("STATS requested cmd=4");
    fetchStats();
  } else if (p.CMD === 5) {
    var idx = (typeof p.ACT_IDX !== "undefined") ? p.ACT_IDX : 0;
    console.log("DETAIL requested cmd=5 idx=" + idx);
    sendActivityDetail(idx);
  }
});

Pebble.addEventListener("ready", function () {
  console.log("Intervals.icu JS ready");
  if (API_KEY) fetchStats();
});
