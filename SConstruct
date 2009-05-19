env = Environment ()
env.Append (CCFLAGS = ['-Wall', '-Wextra'])

if 1:
    env.Append (CCFLAGS = ['-g'])
    env.Append (LINKFLAGS = ['-g'])
else:
    env.Append (CCFLAGS = ['-O2'])
    env.Append (LINKFLAGS = ['-s'])

env.Append (YACCFLAGS = [ '-d', '-v' ])
env.Append (LEXFLAGS = [ '-i' ])

asm_common_srcs = ['asm.c', 'symtab.c', 'util.c', 'release.c']
iasm_srcs = ['iasmy.y', 'iasml.l']
pasm_srcs = ['pasmy.y', 'pasml.l']
psim_srcs = ['psim.c']

asm_common_objs = [env.Object (src) for src in asm_common_srcs]
iasm_objs = [env.Object (src) for src in iasm_srcs]
pasm_objs = [env.Object (src) for src in pasm_srcs]
psim_objs = [env.Object (src) for src in psim_srcs]

iasm = env.Program (target = 'iasm',
                    source = iasm_objs + asm_common_objs)
pasm = env.Program (target = 'pasm',
                    source = pasm_objs + asm_common_objs)

psim = env.Program (target = 'psim',
                    source = psim_objs)

#env.Default (iasm);
env.Default (pasm);
env.Default (psim);
