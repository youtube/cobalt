loadRelativeToScript("utility.js");

for (var line of readFileLines_gen("/tmp/callgraph.txt")) {
  line = line.replace(/\n/, "");
  if (line.contains("xyz"))
    print(line);
}
