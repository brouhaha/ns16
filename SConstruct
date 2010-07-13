import os.path

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
isim_srcs = ['isim.c']
psim_srcs = ['psim.c']

asm_common_objs = [env.Object (src) for src in asm_common_srcs]
iasm_objs = [env.Object (src) for src in iasm_srcs]
pasm_objs = [env.Object (src) for src in pasm_srcs]
isim_objs = [env.Object (src) for src in isim_srcs]
psim_objs = [env.Object (src) for src in psim_srcs]

iasm = env.Program (target = 'iasm',
                    source = iasm_objs + asm_common_objs)
pasm = env.Program (target = 'pasm',
                    source = pasm_objs + asm_common_objs)

def pasm_emitter_fn (target, source, env):
    target.append (os.path.splitext (str (target [0])) [0] + '.lst')
    env.Depends (target, pasm)
    return (target, source)

def pasm_generator_fn (source, target, env, for_signature):
    return '%s %s -o %s -l %s' % (pasm [0].abspath, source [0], target [0], target [1])

pasm_builder = env.Builder (generator = pasm_generator_fn,
                            suffix = '.obj',
                            src_suffix = '.asm',
                            emitter = pasm_emitter_fn)

env.Append (BUILDERS = { 'PASM': pasm_builder })

def iasm_emitter_fn (target, source, env):
    target.append (os.path.splitext (str (target [0])) [0] + '.lst')
    env.Depends (target, iasm)
    return (target, source)

def iasm_generator_fn (source, target, env, for_signature):
    return '%s %s -o %s -l %s' % (iasm [0].abspath, source [0], target [0], target [1])

iasm_builder = env.Builder (generator = iasm_generator_fn,
                            suffix = '.obj',
                            src_suffix = '.asm',
                            emitter = iasm_emitter_fn)

env.Append (BUILDERS = { 'IASM': iasm_builder })

psim = env.Program (target = 'psim',
                    source = psim_objs)

isim = env.Program (target = 'isim',
                    source = isim_objs)

figforth_pace = env.PASM (target = 'figforth_pace.obj',
                          source = 'figforth_pace.asm')

figforth_imp16 = env.IASM (target = 'figforth_imp16.obj',
                           source = 'figforth_imp16.asm')

env.Default (iasm);
env.Default (pasm);
env.Default (isim);
env.Default (psim);
env.Default (figforth_pace);
env.Default (figforth_imp16);
