package dev.cybercraft.tweaks.paths;

import java.util.concurrent.atomic.AtomicLong;

import dev.cybercraft.tweaks.CyberCraftTweaks;

/**
 * What the pathfinding mixins did, for one line in Minecraft's log each minute in which they did anything: the city's
 * answers, the shapes classified, the sight and shortcut casts with their hits and time, and the pets landed on the
 * city's floor. The counters are atomic, so any thread may count; only the server's tick writes the line.
 */
public final class PathStats {
    private static final long MINUTE = 60_000_000_000L;

    private static final AtomicLong ANSWERS = new AtomicLong();
    private static final AtomicLong CLASSIFIED = new AtomicLong();
    private static final AtomicLong SIGHT_CASTS = new AtomicLong();
    private static final AtomicLong SIGHT_HITS = new AtomicLong();
    private static final AtomicLong SHORTCUT_CASTS = new AtomicLong();
    private static final AtomicLong SHORTCUT_HITS = new AtomicLong();
    private static final AtomicLong CAST_NANOS = new AtomicLong();
    private static final AtomicLong PETS = new AtomicLong();

    private static long nextLine;  // the server thread's alone

    private PathStats() {
    }

    public static void answered() {
        ANSWERS.incrementAndGet();
    }

    public static void classified() {
        CLASSIFIED.incrementAndGet();
    }

    public static void cast(boolean sight, boolean hit, long nanos) {
        (sight ? SIGHT_CASTS : SHORTCUT_CASTS).incrementAndGet();
        if (hit) {
            (sight ? SIGHT_HITS : SHORTCUT_HITS).incrementAndGet();
        }
        CAST_NANOS.addAndGet(nanos);
    }

    public static void petLanded() {
        PETS.incrementAndGet();
    }

    /** Each server tick: once a minute, the line, if anything was counted. */
    public static void tick() {
        long now = System.nanoTime();
        if (nextLine == 0) {
            nextLine = now + MINUTE;
            return;
        }
        if (now < nextLine) {
            return;
        }
        nextLine = now + MINUTE;
        long answers = ANSWERS.getAndSet(0), classified = CLASSIFIED.getAndSet(0);
        long sight = SIGHT_CASTS.getAndSet(0), sightHits = SIGHT_HITS.getAndSet(0);
        long shortcuts = SHORTCUT_CASTS.getAndSet(0), shortcutHits = SHORTCUT_HITS.getAndSet(0);
        long nanos = CAST_NANOS.getAndSet(0), pets = PETS.getAndSet(0);
        if (answers + classified + sight + shortcuts + pets == 0) {
            return;
        }
        CyberCraftTweaks.LOG.info("CyberCraft Tweaks: paths in the last minute: {} city answers, {} shapes classified; "
            + "{} sight casts ({} hit), {} shortcut casts ({} hit), {} ms casting; {} pets landed on the city's floor",
            answers, classified, sight, sightHits, shortcuts, shortcutHits, String.format("%.1f", nanos / 1e6), pets);
    }
}
