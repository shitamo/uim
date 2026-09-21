/*

  Copyright (c) 2003-2013 uim Project https://github.com/uim/uim

  All rights reserved.

  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions
  are met:

  1. Redistributions of source code must retain the above copyright
     notice, this list of conditions and the following disclaimer.
  2. Redistributions in binary form must reproduce the above copyright
     notice, this list of conditions and the following disclaimer in the
     documentation and/or other materials provided with the distribution.
  3. Neither the name of authors nor the names of its contributors
     may be used to endorse or promote products derived from this software
     without specific prior written permission.

  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS ``AS IS'' AND
  ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
  ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT HOLDERS OR CONTRIBUTORS BE LIABLE
  FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
  DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
  OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
  HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
  LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
  OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
  SUCH DAMAGE.

*/

/*
 * uim-dbus-ic.h -- D-Bus interface constants for the Phase 2 (proposed)
 * per-InputContext D-Bus architecture.
 *
 * NOTHING IN THIS FILE IS WIRED UP YET. No .c file in the tree implements
 * or calls any of this. It exists so the interface shape can be reviewed,
 * referenced from documentation, and iterated on in isolation before any
 * daemon or im module code is written against it. See
 * doc/dbus-helper-bus.md, "Phase 2", for the rationale and the staged
 * plan this header is step 1 of.
 *
 * This is a *proposal*, not a committed ABI: interface names, method
 * signatures and object path shapes may still change before Phase 2
 * step 2 (making uim-agent actually implement this) lands.
 *
 * Naming: deliberately uim's own namespace (org.uim.*) rather than
 * IBus's (org.freedesktop.IBus.*). uim is not attempting IBus wire
 * compatibility -- see doc/dbus-helper-bus.md for why.
 */

#ifndef UIM_DBUS_IC_H
#define UIM_DBUS_IC_H

/*
 * --- org.uim.Factory1 -------------------------------------------------
 *
 * One Factory object per session, owned by the per-session agent daemon
 * (the eventual D-Bus-activated successor to today's fork/exec'd
 * uim-agent). Its bus name and object path are fixed and well-known, so
 * an im module can find it without any prior IPC, the same way an
 * IBus im module finds ibus-daemon's Bus object.
 *
 * Methods:
 *   CreateInputContext(client_name: s) -> (ic_path: o)
 *     Creates a new input context for the calling client and returns the
 *     object path of the new org.uim.InputContext1 object. client_name
 *     is a short, human-readable identifier for the requesting toolkit
 *     bridge (e.g. "gtk3-immodule", "qt5-immodule", "xim"), used only
 *     for logging/diagnostics.
 *
 *   ListEngines() -> (names: as)
 *     Returns the names of the input method engines (uim's existing
 *     concept, e.g. "anthy", "skk") available in this session, so a
 *     toolkit-side engine switcher UI doesn't need its own separate
 *     path into uim-agent to enumerate them.
 *
 * Signals: (none in v1 -- engine-list changes, if ever needed, would be
 * a future EnginesChanged() addition, not required for the Phase 2
 * step 1-3 plan.)
 */
#define UIM_DBUS_IC_BUS_NAME          "org.uim.Agent"
#define UIM_DBUS_IC_FACTORY_PATH      "/org/uim/Factory"
#define UIM_DBUS_IC_FACTORY_INTERFACE "org.uim.Factory1"

#define UIM_DBUS_IC_FACTORY_METHOD_CREATE_INPUT_CONTEXT "CreateInputContext"
#define UIM_DBUS_IC_FACTORY_METHOD_LIST_ENGINES          "ListEngines"

/*
 * --- org.uim.InputContext1 ---------------------------------------------
 *
 * One object per focused (or recently-focused) input context, created on
 * demand by Factory1.CreateInputContext() and destroyed by its own
 * Destroy() method. Object paths are agent-assigned, of the shape
 * "/org/uim/InputContext/<n>" (monotonically increasing integer per
 * agent process lifetime) -- callers must treat the path as opaque and
 * not construct or parse it themselves, mirroring how IBus's own
 * InputContext paths work.
 *
 * Methods (im module -> agent; all are plain method calls, replacing
 * the bespoke framed messages uim/agent.c currently writes over a pipe):
 *
 *   ProcessKeyEvent(keyval: u, keycode: u, state: u) -> (handled: b)
 *     keyval/keycode/state carry the same information
 *     uim_ic_press_key()/uim_ic_release_key() do today (X11/XKB
 *     keysym, hardware keycode, modifier mask; state's high bit is
 *     reused as today's "is release" flag). Returns whether the engine
 *     consumed the event.
 *
 *   SetCursorLocation(x: i, y: i, w: i, h: i)
 *   SetCursorLocationRelative(x: i, y: i, w: i, h: i)
 *     Same split that today's HELPER-PROTOCOL "cursor_location" /
 *     "cursor_location_relative" messages encode; kept as two methods
 *     rather than one with a "relative: b" flag so the common
 *     absolute-position call stays a 4-argument call, not 5.
 *
 *   FocusIn()
 *   FocusOut()
 *     Same semantics as today's helper-bus "focus_in"/"focus_out".
 *
 *   Reset()
 *     Discards any in-progress composition, same as today's "reset".
 *
 *   SetCapabilities(caps: u)
 *     Bitmask of client capabilities (preedit display, surrounding
 *     text, ...); mirrors the purpose of IBus's own SetCapabilities,
 *     values are uim's own bit assignments (TBD when this is actually
 *     implemented, not fixed by this header).
 *
 *   SetEngine(name: s)
 *     Switches this input context to the named engine (from
 *     Factory1.ListEngines()'s result), same effect as today's
 *     im-switcher action.
 *
 *   Destroy()
 *     Releases the input context and its D-Bus object. The agent also
 *     does this automatically if the caller disconnects from the bus
 *     without calling it (tracked via DBusConnection disconnect
 *     watches, same idea as IBus's own IC cleanup-on-disconnect).
 *
 * Signals (agent -> im module, emitted on the InputContext object's own
 * path so a client only has to subscribe to signals from the specific
 * object path(s) it owns):
 *
 *   CommitText(text: s)
 *   UpdatePreeditText(text: s, cursor_pos: u, visible: b)
 *   UpdateAuxString(text: s, visible: b)
 *   UpdateLookupTable(candidates: as, cursor_pos: i, visible: b)
 *     v1 keeps the lookup table as a flat string array plus a cursor
 *     index rather than trying to carry over every field
 *     HELPER-PROTOCOL's candidate-window messages send (per-candidate
 *     annotation strings, paging, layout hints); those can be added as
 *     additional signal arguments later without renaming the signal,
 *     once step 3 (porting a real im module) shows which of them are
 *     actually load-bearing.
 *   Enabled()
 *   Disabled()
 *     Engine enabled/disabled notifications, same events today's
 *     helper-bus "im_change_list"/prop-list updates cover indirectly.
 */
#define UIM_DBUS_IC_INTERFACE "org.uim.InputContext1"
#define UIM_DBUS_IC_PATH_PREFIX "/org/uim/InputContext/"

#define UIM_DBUS_IC_METHOD_PROCESS_KEY_EVENT           "ProcessKeyEvent"
#define UIM_DBUS_IC_METHOD_SET_CURSOR_LOCATION         "SetCursorLocation"
#define UIM_DBUS_IC_METHOD_SET_CURSOR_LOCATION_RELATIVE "SetCursorLocationRelative"
#define UIM_DBUS_IC_METHOD_FOCUS_IN                    "FocusIn"
#define UIM_DBUS_IC_METHOD_FOCUS_OUT                   "FocusOut"
#define UIM_DBUS_IC_METHOD_RESET                       "Reset"
#define UIM_DBUS_IC_METHOD_SET_CAPABILITIES            "SetCapabilities"
#define UIM_DBUS_IC_METHOD_SET_ENGINE                  "SetEngine"
#define UIM_DBUS_IC_METHOD_DESTROY                     "Destroy"

#define UIM_DBUS_IC_SIGNAL_COMMIT_TEXT           "CommitText"
#define UIM_DBUS_IC_SIGNAL_UPDATE_PREEDIT_TEXT   "UpdatePreeditText"
#define UIM_DBUS_IC_SIGNAL_UPDATE_AUX_STRING     "UpdateAuxString"
#define UIM_DBUS_IC_SIGNAL_UPDATE_LOOKUP_TABLE   "UpdateLookupTable"
#define UIM_DBUS_IC_SIGNAL_ENABLED               "Enabled"
#define UIM_DBUS_IC_SIGNAL_DISABLED              "Disabled"

#endif /* UIM_DBUS_IC_H */
