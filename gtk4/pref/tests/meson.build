# Tests for uim-pref-gtk4.
#
# The test programs #include the .c files of uim-pref-gtk4 so that they
# can reach its static functions without changing the program itself.

pref_test_c_args = [
  '-DPREF_TEST_FIXTURE="@0@"'.format(meson.current_source_dir() /
                                     'fixture.scm'),
  '-DUIM_DATADIR="@0@"'.format(prefix / pkgdatadir),
]
pref_test_env = uim_build_tree_env + {
  'LIBUIM_VANILLA': '1',
  'LC_ALL': 'C.UTF-8',
  # a failed g_return_if_fail() or any other critical fails the test
  'G_DEBUG': 'fatal-criticals',
}

pref_test_glib = dependency('glib-2.0')

# Tests that need no display.
foreach name : ['keytab', 'custom']
  exe = executable('test-pref-' + name,
                   'test-pref-@0@.c'.format(name),
                   c_args: pref_test_c_args,
                   dependencies: [pref_test_glib],
                   include_directories: uim_include_directories,
                   link_with: [libuim_custom, libuim_scm, libuim])
  test('pref-' + name,
       exe,
       env: pref_test_env,
       protocol: 'tap',
       args: ['--tap'],
       suite: 'gtk4-pref')
endforeach

# Tests that need a display. They run under xvfb-run when it is
# installed; otherwise they use the current display, and report
# themselves as skipped when there is none.
xvfb_run = find_program('xvfb-run', required: false)
pref_test_gtk_env = pref_test_env + {
  # keep the tests away from the session's accessibility bus and
  # settings daemon
  'GTK_A11Y': 'none',
  'NO_AT_BRIDGE': '1',
  'GSETTINGS_BACKEND': 'memory',
  # software rendering: no GL warnings or differences under Xvfb
  'GSK_RENDERER': 'cairo',
  # Mesa warns about the missing DRI3 when GDK probes EGL on Xvfb, and
  # xvfb-run passes stderr into the TAP stream on stdout
  'EGL_LOG_LEVEL': 'fatal',
}
if xvfb_run.found()
  pref_test_gtk_env += {'GDK_BACKEND': 'x11'}
endif

pref_test_gtk_programs = {
  'widgets': ['test-pref-widgets.c', '../key-util-gtk4.c'],
  'window': [
    'test-pref-window.c',
    '../gtk-custom-widgets-gtk4.c',
    '../key-util-gtk4.c',
  ],
}
# Real key events are injected with XTest.
xtst = dependency('xtst', required: false)
if x11.found() and xtst.found()
  pref_test_gtk_programs += {
    'keyevent': ['test-pref-keyevent.c', '../key-util-gtk4.c'],
  }
endif

foreach name, sources : pref_test_gtk_programs
  exe = executable('test-pref-' + name,
                   sources,
                   c_args: pref_test_c_args + ['-Wno-deprecated-declarations'],
                   dependencies: [gtk4, x11, xtst],
                   include_directories: uim_include_directories,
                   link_with: gtk4_pref_link_with)
  if xvfb_run.found()
    test('pref-' + name,
         xvfb_run,
         args: ['-a', '-s', '-screen 0 1024x768x24', exe, '--tap'],
         depends: exe,
         env: pref_test_gtk_env,
         protocol: 'tap',
         # xvfb-run -a picks a free display number racily
         is_parallel: false,
         suite: 'gtk4-pref')
  else
    test('pref-' + name,
         exe,
         args: ['--tap'],
         env: pref_test_gtk_env,
         protocol: 'tap',
         suite: 'gtk4-pref')
  endif
endforeach
