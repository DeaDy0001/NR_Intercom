#!/bin/sh
# Kopiert die Build-Ergebnisse nach firmware/bin:
#   <name>-bootloader.bin, -partitions.bin, -otadata.bin, -app.bin -> USB-Flashen mit NR-Intercom.bat (Einstellungen bleiben)
#   <name>-app.bin allein                                           -> Update ueber WLAN
#   <name>.bin (Komplett-Abbild ab 0x0)                            -> nur fuer das Flashen im Browser (setzt alles zurueck)
# Aufruf nach "idf.py build" aus dem Repository-Wurzelordner:  sh tools/export_bins.sh
set -e
cd "$(dirname "$0")/../firmware"
for spec in bodypack-c5:nr-bodypack-c5:nr_bodypack_c5 display-s3:nr-display-s3:nr_display_s3; do
    dir=${spec%%:*}; rest=${spec#*:}; name=${rest%%:*}; app=${rest#*:}
    [ -f "$dir/build/$app.bin" ] || { echo "uebersprungen: $dir (nicht gebaut)"; continue; }
    # zuerst merge-bin (baut ggf. neu), danach die Einzeldateien aus demselben Stand kopieren
    (cd "$dir" && idf.py merge-bin -o "$name.bin" >/dev/null 2>&1 && cp "build/$name.bin" ../bin/)
    cp "$dir/build/bootloader/bootloader.bin"          "bin/$name-bootloader.bin"
    cp "$dir/build/partition_table/partition-table.bin" "bin/$name-partitions.bin"
    cp "$dir/build/$app.bin"                           "bin/$name-app.bin"
    cp "$dir/build/ota_data_initial.bin"               "bin/$name-otadata.bin"
    echo "exportiert: $name"
done
