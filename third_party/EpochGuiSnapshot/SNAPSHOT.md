# EpochGui Snapshot

This bounded source snapshot contains only the renderer-neutral rounded-rectangle geometry and embedded 5x7 bitmap font required by the testbed.

Source repository: `Autodidac/EpochGui`

Pinned source commit: `347ad52e8fc27deb08dea97e56a9b6d8c0db3af2`

The production namespaces and algorithms remain `epochengine::gui_lib`, `epochengine::gui_lib::rounded_rect`, and `epochengine::gui_lib::font`. The conventional header/source form avoids a GCC 14 internal compiler error in the full EpochGui C++ module build while keeping the integration cross-platform and limited to the features actually used.
