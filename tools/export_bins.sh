#!/bin/sh
# Kopiert die Build-Ergebnisse nach firmware/bin:
#   <name>-bootloader.bin, <name>-partitions.bin, <name>-app.bin  -> fuer flash-*.bat (Einstellungen bleiben erhalten)
#   <name>.bin (Komplett-Abbild ab 0x0)                            -> nur fuer das Flashen im Browser (setzt alles zurueck)
# Aufruf nach "idf.py build" aus dem Repository-Wurzelordner:  sh tools/export_bins.sh
set -e
cd "$(dirname "$0")/../firmware"
for spec in bodypack-c5:nr-bodypack-c5:nr_bodypack_c5 display-s3:nr-display-s3:nr_display_s3; do
    dir=${spec%%:*}; rest=${spec#*:}; name=${rest%%:*}; app=${rest#*:}
    [ -f "$dir/build/$app.bin" ] || { echo "uebersprungen: $dir (nicht gebaut)"; continue; }
    cp "$dir/build/bootloader/bootloader.bin"          "bin/$name-bootloader.bin"
    cp "$dir/build/partition_table/partition-table.bin" "bin/$name-partitions.bin"
    cp "$dir/build/$app.bin"                           "bin/$name-app.bin"
    (cd "$dir" && idf.py merge-bin -o "$name.bin" >/dev/null && cp "build/$name.bin" ../bin/)
    echo "exportiert: $name"
done
