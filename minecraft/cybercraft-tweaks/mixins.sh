#!/usr/bin/env bash
# Checks the mod's mixins on Minecraft's own classes (those naming their target class, @Mixin(value = ...)) against
# Minecraft 26.3's jar, as Mixin would when Minecraft loads them, so a wrong target fails here rather than at
# Minecraft's start (tools/MixinCheck.java says what it checks). It reads the classes build.sh compiles, so run that
# first. It needs the JDK alone, and the ASM jars the Modrinth App keeps; the last line counts checks and problems.
set -euo pipefail
cd "$(dirname "$0")"
modrinth="$(cygpath -u "$APPDATA")/ModrinthApp"
newest() { ls $1 | sort -V | tail -1; }  # the newest of the files a pattern names
minecraft=$(newest "$modrinth/meta/versions/26.3-*/26.3-*.jar")
asm=""
for name in asm asm-tree; do
	asm+="$(cygpath -w "$(newest "$modrinth/meta/libraries/org/ow2/asm/$name/*/$name-*.jar")");"
done
rm -rf build/mixincheck
mkdir -p build/mixincheck
javac --release 21 -Xlint:all -cp "$asm" -d build/mixincheck tools/MixinCheck.java
classes=()
for source in $(grep -l "@Mixin(value = " src/main/java/dev/cybercraft/tweaks/mixin/*.java); do
	classes+=("$(cygpath -w "build/classes/dev/cybercraft/tweaks/mixin/$(basename "$source" .java).class")")
done
java -cp "$asm$(cygpath -w build/mixincheck)" MixinCheck "$(cygpath -w "$minecraft")" "${classes[@]}"
