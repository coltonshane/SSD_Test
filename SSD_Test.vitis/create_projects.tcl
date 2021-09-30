file copy -force "SSD_Test/src/lscript.ld" "lscript_temp.ld"
file copy -force "SSD_Test/src/README.txt" "README_temp.txt"
app create -name "SSD_Test" -template {Empty Application}
app config -name "SSD_Test" libraries m
file copy -force "lscript_temp.ld" "SSD_Test/src/lscript.ld"
file copy -force "README_temp.txt" "SSD_Test/src/README.txt"
 
file delete -force "lscript_temp.ld"
file delete -force "README_temp.txt"