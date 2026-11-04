# Tests for uim-dict-gtk4.
#
# The tests compile the dictionary utility's own sources directly (there is
# no internal library to link against) and never touch the real user
# dictionary: every test that needs a backend works on a private dictionary
# placed in a temporary directory (see test-dict-common.c).

dict_tests_dir = meson.current_source_dir()
dict_src_dir = dict_tests_dir / '..'

dict_test_glib = dependency('glib-2.0')

dict_test_include_directories = [
  uim_include_directories,
  include_directories('..'),
]

dict_test_env = environment()
dict_test_env.set('LC_ALL', 'C.UTF-8')
dict_test_env.set('LANGUAGE', 'C')
dict_test_env.set('GSETTINGS_BACKEND', 'memory')
dict_test_env.set('GTK_A11Y', 'none')
dict_test_env.set('NO_AT_BRIDGE', '1')
dict_test_env.set('G_DEBUG', 'fatal-criticals')

# 1. Pure logic: word list handling, charset conversion, part-of-speech
#    tables and the uim_dict dispatch layer (dict.c is built without any
#    backend, a fake in-memory class is used instead).
test_dict_core = executable(
  'test-dict-core',
  [
    'test-dict-core.c',
    'test-dict-common.c',
    'test-fake-dict.c',
    dict_src_dir / 'canna-cclass.c',
    dict_src_dir / 'dict.c',
    dict_src_dir / 'util.c',
    dict_src_dir / 'word.c',
  ],
  dependencies: [dict_test_glib],
  include_directories: dict_test_include_directories,
  install: false)
test('test-dict-core',
     test_dict_core,
     env: dict_test_env,
     protocol: 'tap',
     args: ['--tap'],
     suite: ['dict', 'gtk4'])

# 2. The Anthy backend (anthy.c) against a private dictionary in a
#    temporary $HOME.
if dict_anthy.found()
  test_dict_anthy = executable(
    'test-dict-anthy',
    [
      'test-dict-anthy.c',
      'test-dict-common.c',
      dict_src_dir / 'anthy.c',
      dict_src_dir / 'canna-cclass.c',
      dict_src_dir / 'dict.c',
      dict_src_dir / 'util.c',
      dict_src_dir / 'word.c',
    ],
    c_args: ['-DUSE_ANTHY'],
    dependencies: [dict_test_glib, dict_anthy, dl],
    include_directories: dict_test_include_directories,
    install: false)
  test('test-dict-anthy',
       test_dict_anthy,
       env: dict_test_env,
       protocol: 'tap',
       args: ['--tap'],
       is_parallel: false,
       suite: ['dict', 'gtk4'])
endif

# 3. Widgets. These need a display; they run under xvfb-run when it is
#    available and skip themselves (exit status 77) when no display can be
#    opened.
dict_gtk_test_sources = [
  'test-dict-gtk.c',
  'test-dict-common.c',
  'test-fake-dict.c',
]
foreach source : dict_gtk4_sources
  if source != 'main-gtk4.c'
    dict_gtk_test_sources += [dict_src_dir / source]
  endif
endforeach

test_dict_gtk = executable(
  'test-dict-gtk',
  dict_gtk_test_sources,
  c_args: dict_gtk4_c_args + ['-DGDK_DISABLE_DEPRECATION_WARNINGS'],
  dependencies: [gtk4, dict_anthy, dict_canna, dl],
  include_directories: dict_test_include_directories,
  link_with: [libuim_scm, libuim],
  install: false)

xvfb_run = find_program('xvfb-run', required: false)
if xvfb_run.found()
  test('test-dict-gtk',
       find_program('run-xvfb.sh'),
       args: [test_dict_gtk, '--tap'],
       env: dict_test_env,
       protocol: 'tap',
       is_parallel: false,
       timeout: 120,
       suite: ['dict', 'gtk4'])
else
  test('test-dict-gtk',
       test_dict_gtk,
       args: ['--tap'],
       env: dict_test_env,
       protocol: 'tap',
       is_parallel: false,
       timeout: 120,
       suite: ['dict', 'gtk4'])
endif
