#!/usr/bin/env bash
# Runs CyberCraft Tweaks' unit tests: the pure rules (CellRule), compiled with the JDK alone and run with plain java, no
# JUnit and no Minecraft. The last line counts the checks and the failures, and a failure makes it exit 1.
set -euo pipefail
cd "$(dirname "$0")"
rm -rf build/test
mkdir -p build/test
javac --release 21 -Xlint:all -d build/test src/main/java/dev/cybercraft/tweaks/paths/CellRule.java \
	$(find src/test/java -name '*.java')
java -cp build/test dev.cybercraft.tweaks.paths.CellRuleTest
