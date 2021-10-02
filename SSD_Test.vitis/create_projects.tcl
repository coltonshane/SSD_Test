file copy -force "SSD_Test/src/lscript.ld" "lscript_temp.ld"
app create -name "SSD_Test" -template {Empty Application(C)}
app config -name "SSD_Test" libraries m
file copy -force "lscript_temp.ld" "SSD_Test/src/lscript.ld" 
file delete -force "lscript_temp.ld"
