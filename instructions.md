# Skeleton Passthrough

Forwards one input artifact to its output unchanged. This stage exists as a
worked example for external plugin authors: it demonstrates the minimal
Decode-Orc 2.x stage contract, the structured preview capability, and this
help text itself (see "About this help page" below).

## When to use

Use Skeleton Passthrough as a starting point for writing your own external
transform stage, or insert it into a pipeline as a neutral placeholder while
wiring up a processing graph. It never modifies sample data, so adding or
removing it has no effect on the output.

## What it does

The stage takes the first input artifact and returns it as its only output.
When the input is a `VideoFrameRepresentation` (frame-based CVBS_U10_4FSC
data), the stage also caches it and declares a preview capability from it, so
the host preview dialog can display the passthrough signal: data type
(composite or Y/C, PAL or NTSC), navigable frame range, and active picture
geometry.

## Parameters

- **Note** (`note`, text, default empty): free text kept with the node. The
  stage does nothing with it. The Skeleton Inspector's **Keep as Note**
  writes its last measurement here.

## Skeleton Inspector

**Stage Tools > Skeleton Inspector** opens the plugin's own window (in
orc-gui, when the plugin's view binary is installed beside the stage):

- The summary says how many frames the stage last forwarded, and the note.
  **Refresh** asks again.
- **Measure** counts the dropouts the input reports over every frame, with
  a progress bar; **Cancel** stops it. Previews carry on while it runs.
- **Keep as Note** writes the measurement to the **Note** parameter, the
  same edit as making it in the parameter editor.

## About this help page

This text is rendered by the host when you open the stage's help. It comes
from the `instructions.md` file shipped alongside the plugin shared library:

- The stage class adds the SDK's `ORC_STAGE_INSTRUCTIONS_MD` macro to its
  public section, which implements `DAGStage::get_instructions()` by locating
  the plugin library at runtime and reading the Markdown file next to it.
- The `orc_add_stage_plugin()` CMake helper copies `instructions.md` from the
  plugin source directory to `lib<plugin>.md` next to the built library and
  installs it with the plugin.

To document your own stage, edit this file — no C++ changes are needed once
the macro is in place.
