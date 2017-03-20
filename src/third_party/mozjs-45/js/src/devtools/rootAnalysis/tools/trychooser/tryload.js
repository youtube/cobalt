var TRY_BUILD_LOAD_URL =  "http://builddata.pub.build.mozilla.org/reports/pending/pending_compile_try.txt";
var TEST_LOAD_URL =  "http://builddata.pub.build.mozilla.org/reports/pending/pending_test_all.txt";

/* Initialize the things which appear in the right-hand side of assignments in getTryLoads with the names
   used in the URLs above. */
function initLoad() {
  return {
    // compile
    "linux64": 0,
    "macosx64": 0,
    "win64": 0,
    // test
    "ubuntu32-hw": 0,
    "ubuntu32-vm": 0,
    "ubuntu64-hw": 0,
    "ubuntu64-vm": 0,
    "mac10.6": 0,
    "mac10.10": 0,
    "winxp-ix": 0,
    "win7-ix": 0,
    "win8-ix": 0,
    "win2012x64": 0,
    "panda": 0,
  };
}

function getTryLoad(url, callback) {
  var oXHR = new XMLHttpRequest();
  oXHR.onreadystatechange = function (e) {
    if (oXHR.readyState === 4 && (oXHR.status === 200 || oXHR.status === 0)) {
      var loadText = oXHR.responseText;
      var loadLines = loadText.split("\n");
      var load = initLoad();
      for (var i in loadLines) {
        var line = loadLines[i];
        var words = loadLines[i].split(" ");
        if (words.length == 2 && words[1].charAt(0) == "(" && words[1].charAt(words[1].length-1) == ")") {
          load[words[0]] = parseInt(words[1].substring(1, words[1].length - 1));
        }
      }
      callback(load);
    }
  }
  oXHR.open("GET", url, true);
  oXHR.responseType = "text";
  oXHR.send(null);
}

function getTryLoads(callback) {
  getTryLoad(TRY_BUILD_LOAD_URL, function(load_try_build) {
    getTryLoad(TEST_LOAD_URL, function(load_test) {
      var totalBuildLoad = {};
      totalBuildLoad["linux"] = load_try_build["linux64"];
      totalBuildLoad["linux64"] = totalBuildLoad["linux"];
      totalBuildLoad["linux64-asan"] = totalBuildLoad["linux"];
      totalBuildLoad["linux64-st-an"] = totalBuildLoad["linux"];
      totalBuildLoad["linux64-valgrind"] = totalBuildLoad["linux"];
      totalBuildLoad["linux64-br-haz"] = totalBuildLoad["linux"];
      totalBuildLoad["linux64-sh-haz"] = totalBuildLoad["linux"];
      totalBuildLoad["linux64-mulet"] = totalBuildLoad["linux"];
      totalBuildLoad["macosx64"] = load_try_build["macosx64"];
      totalBuildLoad["macosx64-st-an"] = load_try_build["macosx64"];
      totalBuildLoad["win32"] = load_try_build["win64"];
      totalBuildLoad["win64"] = totalBuildLoad["win32"];
      totalBuildLoad["android-api-9"] = totalBuildLoad["linux"];
      totalBuildLoad["android-api-11"] = totalBuildLoad["linux"];
      totalBuildLoad["android-x86"] = totalBuildLoad["linux"];
      totalBuildLoad["emulator"] = totalBuildLoad["linux"];
      totalBuildLoad["emulator-jb"] = totalBuildLoad["linux"];
      totalBuildLoad["emulator-kk"] = totalBuildLoad["linux"];
      totalBuildLoad["emulator-l"] = totalBuildLoad["linux"];
      totalBuildLoad["linux32_gecko"] = totalBuildLoad["linux"];
      totalBuildLoad["linux64_gecko"] = totalBuildLoad["linux"];
      totalBuildLoad["macosx64_gecko"] = totalBuildLoad["macosx64"];
      totalBuildLoad["macosx64-mulet"] = totalBuildLoad["macosx64"];
      totalBuildLoad["win32_gecko"] = totalBuildLoad["win32"];
      totalBuildLoad["win32-mulet"] = totalBuildLoad["win32"];
      totalBuildLoad["linux64_graphene"] = totalBuildLoad["linux64"];
      totalBuildLoad["macosx64_graphene"] = totalBuildLoad["macosx64"];
      totalBuildLoad["win64_graphene"] = totalBuildLoad["win64"];
      totalBuildLoad["linux64_horizon"] = totalBuildLoad["linux64"];
      totalBuildLoad["macosx64_horizon"] = totalBuildLoad["macosx64"];
      totalBuildLoad["win64_horizon"] = totalBuildLoad["win64"];

      var totalTestLoad = {};
      totalTestLoad["linux"] = load_test["ubuntu32-hw"] + load_test["ubuntu32-vm"];
      totalTestLoad["linux64"] = load_test["ubuntu64-hw"] + load_test["ubuntu64-vm"];
      totalTestLoad["linux64-mulet"] = totalTestLoad["linux64"];
      totalTestLoad["linux64-asan"] = totalTestLoad["linux64"];
      // linux64-st-an: N/A
      totalTestLoad["macosx64"] = Math.max(load_test["mac10.6"], load_test["mac10.10"]);
      // macosx64-st-an: N/A
      totalTestLoad["macosx64-mulet"] = totalTestLoad["macosx64"];
      totalTestLoad["win32"] = Math.max(load_test["winxp-ix"], load_test["win7-ix"]);
      totalTestLoad["win32-mulet"] = totalTestLoad["win32"];
      totalTestLoad["win64"] = load_test["win8-ix"];
      totalTestLoad["android-api-9"] = load_test["ubuntu64-vm"];
      totalTestLoad["android-api-11"] = load_test["panda"];
      totalTestLoad["android-x86"] = load_test["ubuntu64-hw"];
      totalTestLoad["emulator"] = load_test["ubuntu64-vm"];
      // emulator-jb: N/A
      // emulator-kk: N/A
      // emulator-l: N/A
      totalTestLoad["linux32_gecko"] = totalTestLoad["linux"];
      totalTestLoad["linux64_gecko"] = totalTestLoad["linux64"];
      totalTestLoad["macosx64_gecko"] = totalTestLoad["macosx64"];
      // win32_gecko: N/A
      callback(totalBuildLoad, totalTestLoad);
    });
  });
}

function showTryLoads() {
  getTryLoads(function showLoads(totalBuildLoad, totalTestLoad) {
    for (var platform in totalBuildLoad) {
      var load = totalBuildLoad[platform]
      console.log("build load for: " + platform);
      if (load == undefined) {
        console.log("Load for platform '" + platform + "' not defined. Skipping.");
        continue;
      }

      var elemId = "build_" + platform;
      var elem = document.getElementById(elemId);
      if (!elem) {
        console.log("Element '" + elemId + "' not found. Skipping.");
        continue;
      }

      console.log("build load for: " + platform + ", " + load);
      elem.textContent = load;
      elem.style.color = "rgb(" + Math.min(Math.round((load/500.0) * 255), 255) + ",0,0)";
    }
    for (var platform in totalTestLoad) {
      var load = totalTestLoad[platform]
      console.log("test load for: " + platform);
      if (load == undefined) {
        console.log("Load for platform '" + platform + "' not defined. Skipping.");
        continue;
      }

      var elemId = "test_" + platform;
      var elem = document.getElementById(elemId);
      if (!elem) {
        console.log("Element '" + elemId + "' not found. Skipping.");
        continue;
      }

      console.log("test load for: " + platform + ", " + load);
      elem.textContent = load;
      elem.style.color = "rgb(" + Math.min(Math.round((load/500.0) * 255), 255) + ",0,0)";
    }
  });
}

window.addEventListener('load', function() {
  showTryLoads();
}, false);
