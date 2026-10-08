exec(open("sight.py").read().split("for z in (2003")[0])
for (sx, sz) in ((1290, 2007), (1292, 2007), (1290, 2011)):
    sf = floor(sx, sz, -2)
    eye_s = np.array([sx + 0.5, sf + 1.74, sz + 0.5])
    print(f"skeleton at x {sx} z {sz} floor {sf}")
    for (vx, vz, near) in ((1286, 2007, 5.5), (1286, 2011, 5.5), (1294, 2007, -2), (1296, 2007, -2), (1298, 2007, -3), (1296, 2011, -2), (1300, 2007, -3)):
        vf = floor(vx, vz, near)
        eye_v = np.array([vx + 0.5, vf + 1.62, vz + 0.5])
        print(f"   V at x {vx} z {vz} floor {vf}: {'hid' if hits(eye_s, eye_v) else 'SEE'} ({np.linalg.norm(eye_s - eye_v):.1f} blocks)")
