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

## Phase 2 (proposed, on hold): a real ibus-style architecture

**Status: paused after step 1, pending a decision on whether to pursue
it at all.** The rest of this section, beyond the correction below, is
a proposal that was written before the current architecture had been
checked carefully enough. It's kept for reference, but should not be
read as a committed plan.

### Correction: uim does not have an agent process today

An earlier draft of this section described uim's current architecture
as:

```
application <-> im module (gtk3/qt/xim) <-> uim-agent (fork/exec'd) <-> uim core (scheme/plugins)
```

with a `uim-agent` process handling key events over a per-context
pipe, analogous to how `ibus-daemon` handles them today. **That is not
what the code does.** `uim/agent.c` is an abandoned, never-finished
prototype (its own header comment says "prototype ... TOOOO...
experimental", and its `main()` literally returns before reaching any
of the code that would run it) -- it is not used by, or reachable
from, any real im module.

What every toolkit im module actually does (checked directly in
`gtk3/immodule/gtk-im-uim.c`, which calls `uim_create_context()` and
`uim_press_key()` straight from its own code, and the equivalent is
true of `qt5/immodule`, `xim/`, etc.) is link `libuim.so` and run the
uim core -- the Scheme interpreter (sigscheme) and every IM plugin --
**in-process, inside the application itself.** There is no daemon and
no IPC on the key-event path at all today. The helper bus (both the
socket one and Phase 1's D-Bus transport) is a completely separate,
secondary channel used only for the auxiliary UI processes (candidate
window, toolbar, `uim-pref-*`, `uim-im-switcher`) to exchange
preedit/candidate/property updates with whichever application process
currently has focus -- it was never on the key-processing path.

### Why this changes the scope of Phase 2

The Phase 2 proposal below assumed step 2 was "wire D-Bus onto an
existing daemon." With no existing daemon, step 2 is actually "write a
new, single, long-running process that hosts the uim core (sigscheme +
every plugin) out-of-process for the whole session, and move every im
module from linking `libuim` and calling it in-process to being a thin
D-Bus client of that new process instead." That is a fundamentally
different, much larger undertaking than Phase 1:

- It is not additive in the way Phase 1 was. Phase 1 added a transport
  underneath an unchanged protocol and left every call site alone.
  Doing this for real changes *where uim's core code runs* -- moving
  it out of each application's address space into a shared daemon --
  which affects per-engine state, plugin loading, crash isolation
  (today one application's uim state can't take down another's; a
  shared daemon changes that), and startup latency characteristics,
  not just the wire format.
- It has no small, independently-shippable first slice the way Phase 1
  did (a transport swap you can build, test and fall back from in an
  afternoon). A minimally useful version already means: a new daemon
  binary: D-Bus activation and lifecycle for it; session/IC bookkeeping
  keyed by client + focus; and at least one im module actually ported
  to it end-to-end before any of it is validated against real usage.
- It's the kind of rearchitecture that's worth deciding deliberately,
  not backing into via an incremental patch series, given how large
  the eventual diff is and how central the changed code path is to
  every user of uim.

### What's actually landed, and what isn't

Only interface *definitions* (step 1 in the original numbered plan
below) are implemented -- see the next subsection. No daemon, no
im-module port, and no build/runtime wiring for any of it exists. This
document is being left in place, corrected, as the reference for
*if and when* Phase 2 is picked back up, not as an active plan.

### Step 1 (done): interface definitions

- `uim/uim-dbus-ic.h` defines two interfaces as C constants
  (bus name, object paths, interface names, method/signal names), with
  the full method/signal shapes documented in comments right next to
  each constant.
- `data/dbus-1/interfaces/org.uim.Factory1.xml` and
  `data/dbus-1/interfaces/org.uim.InputContext1.xml` mirror the same
  shapes as standalone D-Bus introspection XML, kept in sync with the
  header by hand -- useful as a reference for anyone inspecting a
  future daemon with `d-feet`/`busctl introspect`/etc., and as a
  design doc in its own right.
- Naming uses uim's own namespace (`org.uim.Agent` bus name,
  `org.uim.Factory1`/`org.uim.InputContext1` interfaces) rather than
  IBus's (`org.freedesktop.IBus.*`). uim is not attempting wire
  compatibility with IBus-aware tooling; sharing IBus's own interface
  names would risk bus-name/object-path collisions with an actual
  `ibus-daemon` running in the same session and would tie uim's
  method/signal shapes to whatever IBus happens to do, for a
  compatibility benefit ("some existing IBus client works against uim
  unmodified") that's speculative until a concrete such client shows
  up wanting it.
- **Nothing calls any of this.** `uim-dbus-ic.h` is not included from
  any `.c` file, and the two `.xml` files aren't read by any code or
  referenced from any `Makefile.am` install rule -- they exist purely
  as a design reference. Interface names, method signatures and object
  path shapes here are a proposal, not a committed ABI, and should be
  expected to change if this is ever picked back up, once a real
  implementation surfaces something this header got wrong.

### Original staged plan (kept for reference; steps 2-4 not started)

The plan below was written assuming an existing agent process could be
incrementally converted. With that premise corrected above, step 2 in
particular needs to be re-scoped (a new daemon, not a converted one)
before any of this is acted on:

1. **(done)** Define the D-Bus interfaces and land them alongside the
   existing agent protocol, unused.
2. ~~Convert `uim-agent` into a D-Bus-activated per-session daemon~~ --
   there is no existing `uim-agent` process to convert (see the
   correction above). A real step 2 means designing and writing a new
   out-of-process daemon from scratch that hosts the uim core and
   exposes `org.uim.Factory1`/`org.uim.InputContext1`, with every
   im module continuing to work unmodified against in-process `libuim`
   in the meantime.
3. Port one im module (the natural choice is `gtk3/immodule`, since
   it's the most actively used) to talk to the new D-Bus interface,
   gated by an environment variable / configure flag, with the
   in-process `libuim` path kept as the default and fallback --
   the same pattern Phase 1 established with `UIM_HELPER_TRANSPORT`.
4. Once that's proven out, port the remaining im modules and the
   UI-side processes, and only then consider retiring the in-process
   path (a multi-release deprecation, not a flag day -- uim supports
   too wide a range of desktop environments and toolkit versions,
   several of them (qt3/tqt) essentially unmaintained upstream, to
   remove the fallback quickly).
