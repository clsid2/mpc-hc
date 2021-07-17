/* jshint browser:true, camelcase:true, curly:true, es3:true, eqeqeq:true,
   immed:true, indent:4, latedef:true, quotmark:double, strict:true, undef:true,
   unused:true */

/* global ActiveXObject */
/* exported controlsInit, positionUpdate, onLoadSnapshot, onAbortErrorSnapshot,
   onCommand, playerInit */


var filePath;
var curPos;
var len;
var state;
var pbr;
var eta;
var volume;
var muted; // 1 no sound
var startTime = new Date().getTime();
var sliderSize = 500;
var sliderButtonWidth = 15;
var vsb = 10;
var vss = 100;
var vs;
var sc = 0;
var rdirt;
var Live;
var re;
var sas;
var cpf;
var cp;
var s;
var m;
var sb1;
var sb2;
var sb3;
var vs1;
var vs2;
var vs3;
var etaup = false;
var httpRequestStatus;


// common functions
function getById(id) {
    "use strict";
    return document.getElementById(id);
}

function getOffsetX(m) {
    "use strict";
    var x = m.offsetLeft;
    while (m.offsetParent) {
        x += (m = m.offsetParent).offsetLeft;
    }
    return x;
}


// controls.html
function timeSyntax(ts) {
    "use strict";
    var b = "";
    for (var a = 0, len = ts.length; a < len; a++) {
        switch (ts.charAt(a)) {
            case "0":
                b += "0";
                break;
            case "1":
                b += "1";
                break;
            case "2":
                b += "2";
                break;
            case "3":
                b += "3";
                break;
            case "4":
                b += "4";
                break;
            case "5":
                b += "5";
                break;
            case "6":
                b += "6";
                break;
            case "7":
                b += "7";
                break;
            case "8":
                b += "8";
                break;
            case "9":
                b += "9";
                break;
            case ".":
                b += ".";
                break;
            case ":":
                b += ":";
                break;
            default:
                break;
        }
    }
    return b;
}

function parseTime(y) {
    "use strict";
    var ts = timeSyntax(y);
    var t = 0;
    var p1 = ts.indexOf(".");
    var p2 = ts.indexOf(":");
    var p3 = ts.indexOf(":", p2 + 1);
    var p4 = ts.indexOf(":", p3 + 1);

    if (p4 !== -1 || (p1 !== -1 && p2 !== -1 && p2 > p1) || (p1 !== -1 && p3 !== -1 && p3 > p1)) {
        return -2000;
    }
    if (p1 === -1) {
        p1 = ts.length + 1;
    } else {
        p1 = p1;
    }
    if (p2 === -1) {
        t = parseFloat((ts + " ").substring(0, p1 + 4));
    }
    if (p2 !== -1 && p3 === -1) {
        t = parseInt(ts.substring(0, p2), 10) * 60 + parseFloat("0" + (ts + " ").substring(p2 + 1, p1 + 4));
    }
    if (p2 !== -1 && p3 !== -1) {
        t = parseInt(ts.substring(0, p2), 10) * 3600 + parseInt(ts.substring(p2 + 1, p3), 10) * 60 + parseFloat("0" + (ts + " ").substring(p3 + 1, p1 + 4));
    }
    return t;
}

function update(a, b) {
    "use strict";
    if (a === -2000) {
        return false;
    }
    if (b) {
        if (a > len && !Live) {
            curPos = len;
        } else {
            curPos = a < 0 ? 0 : a;
        }
        m = curPos * sliderSize / len;
    } else {
        if (a > sliderSize) {
            m = sliderSize;
        } else {
            m = a < 0 ? 0 : a;
        }
        curPos = m * len / sliderSize;
    }
    if (m > sb1.width) {
        sb3.width = sliderSize - Math.floor(m);
        sb1.width = m;
    } else {
        sb1.width = m;
        sb3.width = sliderSize - sb1.width;
    }
    return true;
}

function pad(number, len) {
    "use strict";
    var str = String(number);
    while (str.length < len) {
        str = "0" + str;
    }
    return str;
}

function secondsToTS(a, b) {
    "use strict";
    var a1 = Math.floor(a / 3600000);
    var a2 = Math.floor(a / 60000) % 60;
    var a3 = Math.floor(a / 1000) % 60;
    var a4 = Math.floor(a) % 1000;
    var a1s = pad(a1.toString(), 2);
    var a2s = pad(a2.toString(), 2);
    var a3s = pad(a3.toString(), 2);
    var a4s = pad(a4.toString(), 3);

    switch (b) {
        case 1:
            return a1s;
        case 2:
            return a2s;
        case 3:
            return a3s;
        case 4:
            return a4s;
        case 5:
        case 6:
        case 7:
            return a1s + ":" + a2s + ":" + a3s;
        default:
            return ((a1 > 0 ? (a1s + ":") : "") + a2s + ":" + a3s);
    }
}

function postForm(wmc, ext, extv) {
    "use strict";
    getById("fwmc").value = wmc;
    getById("fextra").value = extv;
    getById("fextra").name = ext;
    getById("ef").submit();
    return true;
}

function autoplay(a) {
    "use strict";
    if (etaup && re.checked === true) {
        etaup = false;
        setTimeout(function () {
            etaup = true;
            if (re.checked === true) {
                postForm(0, "null", 0);
            }
        }, 5000);
    }
    setTimeout(autoplay, rdirt);
    var ct = new Date().getTime();
    var cap = pbr * (ct - startTime);
    if (cap > len && !Live) {
        if (re.checked === true) {
            setTimeout(function () {
                window.location = window.location;
            }, 5000);
        }
    }
    cap = ((cap > len && !Live) ? len : (cap < 0 ? 0 : cap));
    if (sas.checked === true || a === true) {
        update(cap, true);
        cpf.value = secondsToTS(cap, 5);
    }
    var gg = " " + secondsToTS(cap, 5) + " ";
    cp.innerHTML = gg;
    return true;
}

function sliderClick(e) {
    "use strict";
    update((window.event ? window.event.clientX - 3 : e.clientX) + document.body.scrollLeft - getOffsetX(s) - Math.floor(sliderButtonWidth / 2) + sc, false);
    cpf.value = secondsToTS(curPos, 5);
    sas.checked = false;
    return true;
}

function volumeUpdate(a, b) {
    "use strict";
    if (b) {
        if (a > 100) {
            volume = 100;
        } else {
            volume = a < 0 ? 0 : a;
        }
        m = volume * vss / 100;
    } else {
        if (a > vss) {
            m = vss;
        } else {
            m = a < 0 ? 0 : a;
        }
        volume = m * 100 / vss;
    }
    volume = Math.ceil(volume);
    vs1.width = m;
    vs3.width = vss - vs1.width;
    return true;
}

function volSliderClick(e) {
    "use strict";
    var ret = volumeUpdate((window.event ? window.event.clientX - 3 : e.clientX) + document.body.scrollLeft - getOffsetX(vs) - Math.floor(vsb / 2) + sc, false);
    return ret;
}


if (eta === 0) {
    if (state < 0 && filePath.length > 0) {
        eta = 2;
    } else {
        eta = 120;
    }
}

function controlsInit(_filePath, _curPos, _length, _state, _pbr, _eta, _volume, _muted) {
    "use strict";
    filePath = decodeURIComponent(_filePath);
    curPos = _curPos;
    len = _length;
    state = _state;
    pbr = _pbr;
    eta = _eta;
    volume = _volume;
    muted = _muted;

    if (eta > 0) {
        setTimeout(function () {
            etaup = true;
            if (re.checked === true) {
                postForm(0, "null", 0);
            }
        }, 1000 * eta);
    }
    Live = len < 1;
    startTime -= curPos;
    rdirt = len * pbr / sliderSize;
    rdirt = Math.floor(rdirt > 1000 ? 1000 : (rdirt < 300 ? 300 : rdirt));
    cpf = getById("pos");
    cp = getById("time");
    sas = getById("SliderAutoScroll");
    re = getById("reloadenabled");
    s = getById("slider");
    sb1 = getById("c1");
    sb2 = getById("c2");
    sb3 = getById("c3");
    vs = getById("v");
    vs1 = getById("v1");
    vs2 = getById("v2");
    vs3 = getById("v3");

    if (muted === 1) {
        getById("muted").innerHTML = "M";
    }
    vs2.title = volume;
    sb2.title = secondsToTS(curPos, 5);
    s.height = sb1.height = sb2.height = sb3.height = vs.height = vs1.height = vs2.height = vs3.height = 20;
    s.width = sliderSize + (sb2.width = sliderButtonWidth);
    vs.width = vss + (vs2.width = vsb);
    sb1.onclick = sb2.onclick = sb3.onclick = sliderClick;
    vs1.onclick = vs2.onclick = vs3.onclick = volSliderClick;
    sas.checked = true;
    cp.innerHTML = cpf.value = secondsToTS(curPos, 5);
    if (state === 2 && pbr !== 0) {
        autoplay();
    }
    volumeUpdate(volume, true);
    return update(curPos, true);
}

function positionUpdate() {
    "use strict";
    if (event.keyCode < 46 || event.keyCode > 58 || event.keyCode === 47) {
        return false;
    }
    setTimeout(function () {
        update(parseFloat(parseTime(cpf.value)), true);
    }, 1);
    return true;
}


// player.html
function getXMLHTTP() {
    "use strict";
    try {
        return new ActiveXObject("Msxml2.XMLHTTP");
    } catch (e) {
        try {
            return new ActiveXObject("Microsoft.XMLHTTP");
        } catch (err) {}
    }
    if (typeof XMLHttpRequest !== "undefined") {
        return new XMLHttpRequest();
    }
    return null;
}

function makeRequest(req) {
    "use strict";
    var httpRequest = getXMLHTTP();
    try {
        httpRequest.open("GET", req, true);
        httpRequest.send(null);
    } catch (e) {}
}

function onStatus (title, status, pos, posStr, dur, durStr, muted, volume) {
    "use strict";
    var maxTitle = 70;
    var timestr;
    var el;

    if (dur > 0 && posStr && durStr) {
        timestr = posStr + "&nbsp;/&nbsp;" + durStr;
    } else {
        timestr = "&nbsp;";
    }

    if (title.length > maxTitle) {
        title = title.substr(0, maxTitle - 3) + "&hellip;";
    }
    if (!dur || dur === 0) {
        dur = 1;
    }

    el = getById("title");
    if (el) {
        el.innerHTML = title;
    }

    var sbpercent = Math.floor(100 * pos / dur);
    el = getById("seekbarchleft");
    if (el) {
        el.width = sbpercent > 0 ? sbpercent + "%" : "1px";
    }

    el = getById("seekbarchright");
    if (el) {
        el.width = sbpercent < 100 ? (100 - sbpercent) + "%" : "1px";
    }

    el = getById("seekbargrip");
    if (el) {
        el.title = posStr;
    }

    el = getById("status");
    if (el && el.innerHTML !== status) {
        el.innerHTML = status;
    }

    el = getById("timer");
    if (el && el.innerHTML !== timestr) {
        el.innerHTML = timestr;
    }

    el = getById("controlvolumemute");
    if (el) {
        var url = "url(img/controlvolume" + (muted ? "off" : "on") + ".png)";
        if (el.style.backgroundImage !== url) {
            el.style.backgroundImage = url;
        }
    }

    el = getById("controlvolumegrip");
    if (el) {
        el.title = volume;
        volume = (getById("controlvolumebar").offsetWidth - el.offsetWidth) * volume / 100;
        el.style.position = "relative";
        el.style.top = "2px";
        el.style.left = Math.floor(volume) + "px";
    }
}

function onReadyStateChange() {
    "use strict";
    var statusRegExp = /OnStatus\("(.*)", "(.*)", (\d+), "(.*)", (\d+), "(.*)", (\d+), (\d+), "(.*)"\)/;

    if (httpRequestStatus && httpRequestStatus.readyState === 4 && httpRequestStatus.responseText) {
        if (httpRequestStatus.responseText.charAt(0) !== "<") {
            var params = statusRegExp.exec(httpRequestStatus.responseText);
            onStatus(params[1], params[2], parseInt(params[3], 10), params[4], parseInt(params[5], 10), params[6], parseInt(params[7], 10), parseInt(params[8], 10), params[9]);
        } else {
            alert(httpRequestStatus.responseText);
        }
        httpRequestStatus = null;
    }
}

function statusLoop() {
    "use strict";

    if (!httpRequestStatus || httpRequestStatus.readyState === 0) {
        httpRequestStatus = getXMLHTTP();
        try {
            httpRequestStatus.open("GET", "status.html", true);
            httpRequestStatus.onreadystatechange = onReadyStateChange;
            httpRequestStatus.send(null);
        } catch (e) {}
    }
    setTimeout(statusLoop, 500);
}

var snapshotCounter = 0;

function loadSnapshot() {
    "use strict";
    var img = getById("snapshot");
    if (img) {
        img.src = "snapshot.jpg?" + snapshotCounter++;
    }
}

function onLoadSnapshot() {
    "use strict";
    setTimeout(loadSnapshot, 125);
}

function onAbortErrorSnapshot() {
    "use strict";
    setTimeout(loadSnapshot, 5000);
}

function onSeek(e) {
    "use strict";
    var left = 0;
    var right = 0;
    var percent;
    var sb;

    sb = getById("seekbarchleft");
    if (sb) {
        left = getOffsetX(sb);
    }

    sb = getById("seekbarchright");
    if (sb) {
        right = getOffsetX(sb) + sb.offsetWidth;
    }

    sb = getById("seekbargrip");
    if (sb) {
        left += sb.offsetWidth / 2;
        right -= sb.offsetWidth / 2;
    }
    if (left > 0 && left < right) {
        percent = 100 * ((window.event ? window.event.clientX : e.clientX) - left) / (right - left);
        if (percent < 0) {
            percent = 0;
        } else if (percent > 100) {
            percent = 100;
        }
        makeRequest("command.html?wm_command=[setposcommand]&percent=" + percent);
    }
}

function onVolume(e) {
    "use strict";
    var left = 0;
    var right = 0;
    var percent;
    var cv = getById("controlvolumebar");

    if (cv) {
        left = getOffsetX(cv) + 3;
        right = getOffsetX(cv) + cv.offsetWidth - 3;
    }
    if (left > 0 && left < right) {
        percent = 100 * ((window.event ? window.event.clientX : e.clientX) - left) / (right - left);
        if (percent < 0) {
            percent = 0;
        } else if (percent > 100) {
            percent = 100;
        }
        makeRequest("command.html?wm_command=[setvolumecommand]&volume=" + percent);
    }
}

function onCommand(id) {
    "use strict";
    makeRequest("command.html?wm_command=" + id);
}

function playerInit() {
    "use strict";
    statusLoop();
    loadSnapshot();

    var el = getById("seekbar");
    if (el) {
        el.onclick = onSeek;
    }

    el = getById("controlvolumebar");
    if (el) {
        el.onclick = onVolume;
    }
}
