#!/usr/bin/env bash
# Builds CyberCraft Tweaks with the JDK alone, with no Gradle and nothing downloaded: it compiles against the jars the
# Modrinth App already keeps for the CyberCraft instance (Minecraft 26.3 and four of its libraries: the two Fabric API's
# signatures name, slf4j, which the mod logs with, and fastutil, which Minecraft's entities' fields name; Fabric Loader,
# the mixin library, Fabric API's base, lifecycle-events and data-attachment modules, and SkyCraft), and packs
# build/cybercraft-tweaks-<version>.jar.
set -euo pipefail
cd "$(dirname "$0")"
modrinth="$(cygpath -u "$APPDATA")/ModrinthApp"
mods="$modrinth/profiles/CyberCraft/mods"
newest() { ls $1 | sort -V | tail -1; }  # the newest of the files a pattern names
minecraft=$(newest "$modrinth/meta/versions/26.3-*/26.3-*.jar")
# The versions Minecraft 26.3's own version file names.
dfu=$(grep -o 'com.mojang:datafixerupper:[^" ,}]*' "${minecraft%.jar}.json" | head -1 | cut -d: -f3)
jspecify=$(grep -o 'org.jspecify:jspecify:[^" ,}]*' "${minecraft%.jar}.json" | head -1 | cut -d: -f3)
slf4j=$(grep -o 'org.slf4j:slf4j-api:[^" ,}]*' "${minecraft%.jar}.json" | head -1 | cut -d: -f3)
fastutil=$(grep -o 'it.unimi.dsi:fastutil:[^" ,}]*' "${minecraft%.jar}.json" | head -1 | cut -d: -f3)
dfu="$modrinth/meta/libraries/com/mojang/datafixerupper/$dfu/datafixerupper-$dfu.jar"
jspecify="$modrinth/meta/libraries/org/jspecify/jspecify/$jspecify/jspecify-$jspecify.jar"
slf4j="$modrinth/meta/libraries/org/slf4j/slf4j-api/$slf4j/slf4j-api-$slf4j.jar"
fastutil="$modrinth/meta/libraries/it/unimi/dsi/fastutil/$fastutil/fastutil-$fastutil.jar"
loader=$(newest "$modrinth/meta/libraries/net/fabricmc/fabric-loader/*/fabric-loader-*.jar")
mixin=$(newest "$modrinth/meta/libraries/net/fabricmc/sponge-mixin/*/sponge-mixin-*.jar")
fabric_api=$(newest "$mods/fabric-api-*.jar")
skycraft=$(newest "$mods/skycraft-fabric-*.jar")
version=$(sed -n 's/^ *"version": *"\([^"]*\)".*/\1/p' src/main/resources/fabric.mod.json | head -1)
rm -rf build
mkdir -p build/classes build/deps
# Fabric API's modules are jars inside its jar.
unzip -q -j -o "$fabric_api" 'META-INF/jars/fabric-api-base-*.jar' 'META-INF/jars/fabric-lifecycle-events-v1-*.jar' \
  'META-INF/jars/fabric-data-attachment-api-v1-*.jar' -d build/deps
classpath=""
for jar in "$minecraft" "$dfu" "$jspecify" "$slf4j" "$fastutil" "$loader" "$mixin" "$skycraft" build/deps/*.jar; do
	classpath+="$(cygpath -w "$jar");"
done
# -classfile: a Fabric API class names an annotation from a jar it doesn't need at compile time.
javac --release 21 -Xlint:all,-classfile -cp "$classpath" -d build/classes $(find src/main/java -name '*.java')
cp -r src/main/resources/. build/classes/
jar --create --file "build/cybercraft-tweaks-$version.jar" -C build/classes .
echo "built build/cybercraft-tweaks-$version.jar against $(basename "$minecraft"), $(basename "$loader"),"\
" $(basename "$mixin"), $(basename "$fabric_api") and $(basename "$skycraft")"
