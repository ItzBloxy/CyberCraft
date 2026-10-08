import java.io.InputStream;
import java.nio.file.Files;
import java.nio.file.Path;
import java.util.ArrayList;
import java.util.List;
import java.util.Map;
import java.util.HashMap;
import java.util.zip.ZipFile;

import org.objectweb.asm.ClassReader;
import org.objectweb.asm.Opcodes;
import org.objectweb.asm.Type;
import org.objectweb.asm.tree.AbstractInsnNode;
import org.objectweb.asm.tree.AnnotationNode;
import org.objectweb.asm.tree.ClassNode;
import org.objectweb.asm.tree.FieldNode;
import org.objectweb.asm.tree.MethodInsnNode;
import org.objectweb.asm.tree.MethodNode;

/**
 * Checks CyberCraft Tweaks' mixins on Minecraft's own classes against Minecraft's jar, as Mixin would when Minecraft
 * loads them, so a wrong target fails here rather than at Minecraft's start ({@code mixins.sh} runs it): each
 * {@code @Inject}'s and {@code @Redirect}'s target method is there (once, when it's named without a descriptor), its
 * handler's arguments, return and static-ness fit, a redirected call is there at its ordinal, and each
 * {@code @Invoker}'s method and {@code @Shadow} field is there. It knows the kinds the mod uses: {@code @Inject} at
 * {@code HEAD} or {@code RETURN}, {@code @Redirect} of an {@code INVOKE}, and anything else is reported unchecked.
 *
 * <p>Usage: {@code java MixinCheck <minecraft.jar> <mixin class file>...}; the last line counts checks and problems.
 */
public final class MixinCheck {
    static ZipFile jar;
    static final Map<String, ClassNode> CLASSES = new HashMap<>();
    static int problems;
    static int checks;

    static ClassNode load(String internal) throws Exception {
        ClassNode cached = CLASSES.get(internal);
        if (cached != null) {
            return cached;
        }
        var entry = jar.getEntry(internal + ".class");
        if (entry == null) {
            return null;
        }
        ClassNode node = new ClassNode();
        try (InputStream in = jar.getInputStream(entry)) {
            new ClassReader(in).accept(node, 0);
        }
        CLASSES.put(internal, node);
        return node;
    }

    static Object value(AnnotationNode a, String key) {
        if (a == null || a.values == null) {
            return null;
        }
        for (int i = 0; i < a.values.size(); i += 2) {
            if (a.values.get(i).equals(key)) {
                return a.values.get(i + 1);
            }
        }
        return null;
    }

    static AnnotationNode find(List<AnnotationNode> list, String desc) {
        if (list != null) {
            for (AnnotationNode a : list) {
                if (a.desc.equals(desc)) {
                    return a;
                }
            }
        }
        return null;
    }

    static void problem(String where, String what) {
        problems++;
        System.out.println("PROBLEM " + where + ": " + what);
    }

    static void ok(String where, String what) {
        checks++;
        System.out.println("ok " + where + ": " + what);
    }

    static List<MethodNode> targets(ClassNode target, String spec) {
        String name = spec, desc = null;
        int paren = spec.indexOf('(');
        if (paren >= 0) {
            name = spec.substring(0, paren);
            desc = spec.substring(paren);
        }
        List<MethodNode> found = new ArrayList<>();
        for (MethodNode m : target.methods) {
            if (m.name.equals(name) && (desc == null || m.desc.equals(desc))) {
                found.add(m);
            }
        }
        return found;
    }

    /** The calls in {@code m} to {@code ref}, written as Mixin's targets are: {@code Lowner;name(args)return}. */
    static List<MethodInsnNode> calls(MethodNode m, String ref) {
        List<MethodInsnNode> found = new ArrayList<>();
        for (AbstractInsnNode insn : m.instructions) {
            if (insn instanceof MethodInsnNode call && ("L" + call.owner + ";" + call.name + call.desc).equals(ref)) {
                found.add(call);
            }
        }
        return found;
    }

    static FieldNode field(String owner, String name) throws Exception {
        for (String at = owner; at != null; ) {
            ClassNode node = load(at);
            if (node == null) {
                return null;
            }
            for (FieldNode f : node.fields) {
                if (f.name.equals(name)) {
                    return f;
                }
            }
            at = node.superName;
        }
        return null;
    }

    public static void main(String[] args) throws Exception {
        jar = new ZipFile(args[0]);
        for (int i = 1; i < args.length; i++) {
            ClassNode mixin = new ClassNode();
            new ClassReader(Files.readAllBytes(Path.of(args[i]))).accept(mixin, 0);
            String who = mixin.name.substring(mixin.name.lastIndexOf('/') + 1);
            AnnotationNode mix = find(mixin.invisibleAnnotations, "Lorg/spongepowered/asm/mixin/Mixin;");
            @SuppressWarnings("unchecked")
            List<Type> values = (List<Type>) value(mix, "value");
            if (values == null || values.size() != 1) {
                problem(who, "no single @Mixin value");
                continue;
            }
            ClassNode target = load(values.get(0).getInternalName());
            if (target == null) {
                problem(who, "no class " + values.get(0));
                continue;
            }
            for (FieldNode f : mixin.fields) {
                if (find(f.visibleAnnotations, "Lorg/spongepowered/asm/mixin/Shadow;") != null
                    || find(f.invisibleAnnotations, "Lorg/spongepowered/asm/mixin/Shadow;") != null) {
                    FieldNode real = field(target.name, f.name);
                    if (real == null || !real.desc.equals(f.desc)) {
                        problem(who, "@Shadow " + f.name + " " + f.desc + " not in " + target.name);
                    } else {
                        ok(who, "@Shadow " + f.name);
                    }
                }
            }
            for (MethodNode h : mixin.methods) {
                List<AnnotationNode> all = new ArrayList<>();
                if (h.visibleAnnotations != null) {
                    all.addAll(h.visibleAnnotations);
                }
                if (h.invisibleAnnotations != null) {
                    all.addAll(h.invisibleAnnotations);
                }
                for (AnnotationNode a : all) {
                    String kind = a.desc.substring(a.desc.lastIndexOf('/') + 1, a.desc.length() - 1);
                    String where = who + "." + h.name;
                    boolean handlerStatic = (h.access & Opcodes.ACC_STATIC) != 0;
                    if (kind.equals("Invoker")) {
                        String name = (String) value(a, "value");
                        List<MethodNode> found = targets(target, name + h.desc);
                        if (found.size() != 1) {
                            problem(where, "invoker target " + name + h.desc + " found " + found.size());
                        } else if (((found.get(0).access & Opcodes.ACC_STATIC) != 0) != handlerStatic) {
                            problem(where, "invoker static-ness differs");
                        } else {
                            ok(where, "@Invoker " + name + h.desc);
                        }
                        continue;
                    }
                    if (!kind.equals("Inject") && !kind.equals("Redirect")) {
                        if (a.desc.startsWith("Lorg/spongepowered/asm/mixin/injection/") || kind.equals("Accessor")) {
                            problem(where, "@" + kind + " isn't one this check knows");
                        }
                        continue;
                    }
                    @SuppressWarnings("unchecked")
                    List<String> methods = (List<String>) value(a, "method");
                    Object atValue = value(a, "at");
                    AnnotationNode at = atValue instanceof List<?> list ? (AnnotationNode) list.get(0) : (AnnotationNode) atValue;
                    String atKind = (String) value(at, "value");
                    String atTarget = (String) value(at, "target");
                    Integer ordinal = (Integer) value(at, "ordinal");
                    for (String spec : methods) {
                        List<MethodNode> found = targets(target, spec);
                        if (found.size() != 1) {
                            problem(where, "target " + spec + " found " + found.size() + " times in " + target.name);
                            continue;
                        }
                        MethodNode m = found.get(0);
                        boolean targetStatic = (m.access & Opcodes.ACC_STATIC) != 0;
                        if (targetStatic != handlerStatic) {
                            problem(where, "static-ness: target " + targetStatic + ", handler " + handlerStatic);
                        }
                        Type[] targetArgs = Type.getArgumentTypes(m.desc);
                        Type[] handlerArgs = Type.getArgumentTypes(h.desc);
                        if (kind.equals("Inject")) {
                            Type ret = Type.getReturnType(m.desc);
                            String info = ret.getSort() == Type.VOID ? "org/spongepowered/asm/mixin/injection/callback/CallbackInfo"
                                : "org/spongepowered/asm/mixin/injection/callback/CallbackInfoReturnable";
                            boolean fits = handlerArgs.length == targetArgs.length + 1
                                && handlerArgs[targetArgs.length].getInternalName().equals(info)
                                && Type.getReturnType(h.desc).getSort() == Type.VOID;
                            for (int k = 0; fits && k < targetArgs.length; k++) {
                                fits = handlerArgs[k].equals(targetArgs[k]);
                            }
                            if (!fits) {
                                problem(where, "handler " + h.desc + " doesn't fit " + m.name + m.desc);
                            } else if (!atKind.equals("HEAD") && !atKind.equals("RETURN")) {
                                problem(where, "unchecked @At " + atKind);
                            } else {
                                ok(where, "@Inject " + m.name + m.desc + " at " + atKind);
                            }
                        } else {
                            List<MethodInsnNode> calls = atTarget == null ? List.of() : calls(m, atTarget);
                            int need = ordinal == null ? 1 : ordinal + 1;
                            if (!"INVOKE".equals(atKind) || calls.size() < need) {
                                problem(where, atTarget + " occurs " + calls.size() + " times in " + m.name + ", ordinal " + ordinal);
                                continue;
                            }
                            String owner = atTarget.substring(1, atTarget.indexOf(';'));
                            String callDesc = atTarget.substring(atTarget.indexOf('('));
                            Type[] callArgs = Type.getArgumentTypes(callDesc);
                            List<Type> expected = new ArrayList<>();
                            if (calls.get(need - 1).getOpcode() != Opcodes.INVOKESTATIC) {
                                expected.add(Type.getObjectType(owner));  // an instance call's handler takes its receiver first
                            }
                            for (Type t : callArgs) {
                                expected.add(t);
                            }
                            boolean fits = Type.getReturnType(h.desc).equals(Type.getReturnType(callDesc))
                                && handlerArgs.length == expected.size();
                            for (int k = 0; fits && k < expected.size(); k++) {
                                fits = handlerArgs[k].equals(expected.get(k));
                            }
                            if (!fits) {
                                problem(where, "redirect handler " + h.desc + " doesn't fit " + atTarget);
                            } else {
                                ok(where, "@Redirect in " + m.name + " of " + atTarget + " (" + calls.size() + " calls, ordinal " + ordinal + ")");
                            }
                        }
                    }
                }
            }
        }
        System.out.println(checks + " mixin check(s), " + problems + " problem(s)");
        System.exit(problems == 0 ? 0 : 1);
    }
}
