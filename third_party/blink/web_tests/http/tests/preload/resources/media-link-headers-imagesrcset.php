<?php
    header('Link: <http://127.0.0.1:8000/resources/square.png?large>; rel=preload; as=image; imagesrcset="http://127.0.0.1:8000/resources/square.png?small 300w, http://127.0.0.1:8000/resources/square.png?large 600w"; imagesizes="(min-width: 300px) and (max-width: 300px) 300px, 600px"', false);
    header('Link: <http://127.0.0.1:8000/resources/square.png?base>; rel=preload; as=image; imagesrcset="http://127.0.0.1:8000/resources/square.png?299 299w, http://127.0.0.1:8000/resources/square.png?300 300w, http://127.0.0.1:8000/resources/square.png?301 301w"; imagesizes="100vw"', false);
?>
<!DOCTYPE html>
<meta name="viewport" content="width=300">
<script>
    const numPreloads = 2;

    let loaded = [];
    function checkPreloads(perf) {
        for (let e of perf.getEntriesByType('resource')) {
            let q = e.name.indexOf("?");
            if (q >= 0)
                loaded.push(e.name.substr(q + 1));
        }
        if (loaded.length >= numPreloads)
            window.opener.postMessage(loaded, "*");
    }

    let observer = new PerformanceObserver(checkPreloads);
    observer.observe({entryTypes: ["resource"]});
    checkPreloads(performance);
</script>

