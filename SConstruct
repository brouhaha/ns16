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

srcs = ['pasm.c', 'symtab.c', 'util.c', 'release.c', 'pasmy.y', 'pasml.l']
objs = [env.Object (src) for src in srcs]

pasm = env.Program (objs)
