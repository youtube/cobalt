if (window.testRunner)
    testRunner.dumpAsText();

document.getElementById("result").textContent = ("�" == "\u0421") ? "PASS" : "FAIL";
