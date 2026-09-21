# D-Bus support for the uim helper bus

This document describes the current D-Bus transport for the uim "helper
bus", and the roadmap for turning uim into a fully D-Bus-based, ibus-like
input method framework. It's split into what's actually implemented
(Phase 1) and what's proposed for later phases, so it's clear which parts
of this document describe shipped code and which describe a plan.

## Background

Historically, uim's "helper bus" -- the IPC used between an application's
im module (`gtk3/immodule`, `qt5/immodule`, `xim`, ...) and the auxiliary
processes that make up the visible UI (candidate window, toolbar,
`uim-pref-*`, `uim-im-switcher`) -- is `uim-helper-server`: a small daemon
that listens on a Unix domain socket at
`$XDG_RUNTIME_DIR/uim/socket/uim-helper` and rebroadcasts every message it
receives from one client to every other connected client
(`uim-helper-server.c`'s `distribute_message()`). Clients connect to it
via `uim_helper_init_client_fd()`, and if it isn't running yet, the first
client fork/execs it directly (`uim_ipc_open_command()` in
`uim-helper-client.c`).

That "first client spawns it" model is the root of a real, recurring
class of bug: the spawned `uim-helper-server`, and anything it in turn
needs (or that needs *it*, such as an im module loaded by a sandboxed
helper process like `xdg-desktop-portal-gtk`), only sees whatever
environment the spawning process happened to have -- not a
consistently-managed session environment. That's exactly what's behind
the "uim stops working in the portal file picker" class of report:
`xdg-desktop-portal-gtk` runs as a D-Bus/systemd-activated service, so
`GTK_IM_MODULE=uim` set in a login shell never reaches it unless it's
also pushed into `systemd --user`'s or D-Bus activation's own
environment.

Moving the helper bus onto D-Bus session-bus activation attacks this
problem at its source: the daemon is started by dbus-daemon (optionally
via `systemd --user`) using an environment *it* manages, the same way
every other on-demand desktop service (including `xdg-desktop-portal`
itself) is started.

## Phase 1 (implemented): D-Bus transport for the existing protocol

Phase 1 does **not** redesign the helper bus's protocol or semantics. It
swaps the transport underneath the existing one:

- `uim/uim-helper-dbus.c` (new) implements a D-Bus session-bus transport
  that reproduces `uim-helper-server`'s broadcast-to-everyone-but-the-
  sender behavior:
  - The daemon owns the well-known bus name `org.uim.HelperBus` and
    exports `org.uim.HelperBus1` on `/org/uim/HelperBus`, with one method,
    `Send(sender_id: s, message: s)`.
  - On every `Send` call it re-emits the same two strings as a
    `org.uim.HelperBus1.Message(sender_id: s, message: s)` **signal**,
    which the session bus fans out to every subscriber -- that's the
    broadcast that used to be hand-rolled in `distribute_message()`.
  - Each client tags its own outgoing messages with its D-Bus unique
    name and filters out incoming signals carrying that same
    `sender_id`, which reproduces "don't echo back to the sender".
  - The **message text itself is untouched**: `uim_helper_send_message()`
    still terminates every message with a blank line (`"\n\n"`), and the
    D-Bus transport hands received messages straight back into the same
    `uim_helper_buffer_get_message()` parser the socket transport always
    used. Every call site elsewhere in the tree
    (`uim_helper_send_message()`, `uim_helper_get_message()`,
    `uim_helper_client_focus_in/out()`, `uim_helper_client_get_prop_list()`,
    candidate windows, toolbars, `uim-pref-*`, ...) is unchanged. See
    `doc/HELPER-PROTOCOL` for what's actually inside those messages --
    that document still applies as-is.
  - `uim_helper_init_client_fd()` returns the D-Bus connection's
    underlying socket fd (`dbus_connection_get_unix_fd()`), so it drops
    into every existing `select()`-based event loop in the tree (xim,
    the GTK/Qt im modules, the candidate window processes, ...) without
    those loops needing to know the transport changed. That fd must only
    ever be passed back into `uim_helper_send_message()` /
    `uim_helper_read_proc()` / `uim_helper_close_client_fd()`, never
    `read(2)`/`write(2)` directly -- `uim_helper_client_fd_is_dbus()`
    is how those three functions tell which transport a given fd belongs
    to and dispatch accordingly.
- `uim-helper-server`'s `main()` tries `uim_helper_dbus_server_run()`
  first; it only falls back to the legacy socket accept loop if the
  D-Bus session bus itself couldn't be reached at all (headless/console
  sessions, early boot, etc). If another instance already owns
  `org.uim.HelperBus`, it exits quietly, the same one-server-per-session
  invariant the socket path enforced by binding a single well-known path.
- `data/dbus-1/services/org.uim.HelperBus.service.in` makes the daemon
  D-Bus-activatable; `data/systemd/uim-helper-bus.service.in` is an
  optional `systemd --user` unit (installed only when
  `pkg-config systemd --variable=systemduserunitdir` resolves) so
  distributions that manage session services with systemd get proper
  lifecycle management (and crucially: activation-time environment, not
  inherited-from-first-client environment) instead of a bare fork/exec.

### Build

```
./configure --enable-helper-dbus   # default is "auto": on if dbus-1 >= 1.6 is found
make
```

`--enable-helper-dbus=no` (or simply not having `dbus-1` installed)
builds exactly the old socket-only uim, byte-for-byte behaviorally
unchanged -- every function added in Phase 1 compiles to a stub that
returns "not available" when `HAVE_DBUS` isn't defined, so no `#ifdef`
had to be added at any of the existing call sites.

At runtime, `UIM_HELPER_TRANSPORT=socket` in the environment forces the
legacy transport even in a D-Bus-enabled build, which is the escape hatch
during the migration period (and useful for comparing behavior while
debugging).

### Known limitations of Phase 1

These are deliberate simplifications, not oversights -- listed here so
Phase 2 knows what to fix rather than re-discovering it:

- The client side drives the `DBusConnection` with
  `dbus_connection_read_write_dispatch()` off a single fd becoming
  readable, rather than the fully correct
  `dbus_connection_set_watch_functions()`/`set_timeout_functions()`
  integration. This works for the common case (one blocking-style
  select loop per process) but doesn't handle a connection that needs
  to *write* when its fd isn't readable (e.g. a large backlog), and
  ignores D-Bus timeouts entirely. Fine for small, infrequent helper-bus
  messages; not a general-purpose D-Bus main-loop integration.
- `uim-helper-dbus.c` assumes one D-Bus connection per process, matching
  the pre-existing single static `uim_fd` in `uim-helper-client.c`. A
  process that wants a genuinely separate helper-bus connection per
  `uim_context` (the `uc->uim_fd` field already exists for this) doesn't
  get one under the D-Bus transport yet.
- `uim-helper-server`'s D-Bus mode has no idle-exit. The socket server
  used "no clients connected" as a proxy for "user logged out" and
  exited; the D-Bus daemon just keeps running once activated, which is
  actually closer to how ibus-daemon behaves, but was a deliberate
  scope cut rather than a considered design choice to keep it running
  forever. `systemd --user`'s own idle/stop semantics are the more
  natural place to add this back, if it's wanted.
- When another instance already owns `org.uim.HelperBus` (already
  covered) vs. the bus being flat-out unreachable are distinguished, but
  a *third* case -- our own `dbus_bus_request_name()` erroring out for
  some other reason (e.g. policy denial) -- currently also just exits
  the process rather than falling back to the socket transport. That's
  arguably correct (something's actually broken, falling back would
  hide it) but worth a second look once there's real-world data on what
  actually fails in the wild.

## Phase 2 (proposed): a real ibus-style architecture

Phase 1 only moves the existing *bus* onto D-Bus; it does not change how
an application's im module talks to the input method engine itself. That
path is separate and today looks roughly like:

```
application <-> im module (gtk3/qt/xim) <-> uim-agent (fork/exec'd) <-> uim core (scheme/plugins)
```

with `uim/agent.c` and the toolkit-specific bridge code
(`gtk3/immodule`, `qt5/immodule`, `xim/`) each implementing their own
framing over a private pipe/socket pair per input context. That's the
part that would need to change to look like ibus's actual architecture,
where:

- a single long-running daemon (`ibus-daemon` equivalent) owns a D-Bus
  name and exposes a `Bus`/`Factory` object that creates a fresh
  `org.freedesktop.IBus.InputContext`-style object (its own D-Bus object
  path) per focused input context;
- im modules talk to that daemon's D-Bus objects directly (method calls
  for `ProcessKeyEvent`, `FocusIn`, `FocusOut`, `SetCursorLocation`, ...;
  signals for `CommitText`, `UpdatePreeditText`, `UpdateAuxString`,
  `UpdateLookupTable`, ...) instead of going through a bespoke
  pipe-framed protocol;
- panel/candidate-window/toolbar processes are themselves D-Bus clients
  of the daemon rather than peers on a flat broadcast bus, which is what
  actually removes the "everyone sees everyone else's messages and
  filters" pattern Phase 1 still preserves for compatibility.

This is a substantially larger effort than Phase 1: it touches
`uim/agent.c`, every toolkit im module (`gtk3/immodule`, `gtk4/immodule`,
`qt3` through `qt6/immodule`, `tqt/immodule`, `xim/`), and the UI-side
processes (`*/candwin`, `*/toolbar`, `*/pref`, `*/switcher`). A realistic
path there is incremental, in roughly this order, each step independently
shippable and each one able to fall back to the pre-existing mechanism
the same way Phase 1 falls back to the socket transport:

1. Define the D-Bus interfaces (`org.uim.InputContext1`,
   `org.uim.Factory1` or similar -- whether to mirror IBus's own
   interface names for drop-in compatibility with IBus-aware tooling, or
   define uim's own, is an open design question worth settling before
   writing code) and land them alongside the existing agent protocol,
   unused.
2. Convert `uim-agent` into a D-Bus-activated per-session daemon that
   exposes those interfaces *in addition to* its current stdio/pipe
   protocol, proxying to the same core.
3. Port one im module (the natural choice is `gtk3/immodule`, since it's
   the most actively used) to talk to the new D-Bus interface, gated by
   an environment variable / configure flag, with the pipe-based path
   kept as the default and fallback -- exactly the pattern Phase 1
   established with `UIM_HELPER_TRANSPORT`.
4. Once that's proven out, port the remaining im modules and the
   UI-side processes, and only then consider retiring the non-D-Bus
   paths (a multi-release deprecation, not a flag day -- uim supports
   too wide a range of desktop environments and toolkit versions,
   several of them (qt3/tqt) essentially unmaintained upstream, to
   remove the fallback quickly).

None of Phase 2 is implemented yet. Treat the interface names and object
paths above as a starting proposal, not a committed ABI.
