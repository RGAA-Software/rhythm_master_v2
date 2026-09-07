# Popup sizing regression

The user reported that the template browser gradually shrank while open.
The selected Dear ImGui source (`imgui.cpp`, `BeginPopup`) always adds
`AlwaysAutoResize`. The browser supplied its explicit size only on first use,
then placed a child at remaining height minus one pixel. Content autosizing fed
that smaller extent back into the parent on the next frame.

The regression test reproduces the old behavior: `out/popup-regression-before.log`
reports 707 -> 706 pixels. Template, native-node, semantic-library and asset
popups now supply their intended size every frame, bounded by the viewport work
area. Browser children fill the remaining height without a negative offset.
Normal docked/editor windows retain their existing sizing behavior.

`out/popup-fix-tests.log` passes source boundaries and the actual ImGui/D3D11
browser test over 300 frames. It checks thumbnail selection/live preview,
constant width/height, an actual typed query with zero matches, and popup reopening.
No upstream Dear ImGui source was changed. The existing Python deployment targets
remain responsible for copying the complete executable/DLL/resource closure.
