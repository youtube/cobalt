<?php
echo '<meta charset="' . $_GET['charset'] . '">';
echo '<body onload="top.frameLoaded()">';
echo '<p id=charset>' . $_GET['charset'] . '</p>';
echo '<p id=test>SU���SS</p>'; // "���" are Cyrillic characters that look like "CCE".
echo '</body>';
?>
