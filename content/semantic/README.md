# Effect components

Choose **效果组件库 / Effect components** in Studio, search a title and insert it.
Connect the texture output to a compositor or final output. Visible nodes receive
the normal GPU preview. The inspector exposes the controls below and two presets:
Default restores the current component's exposed defaults; the variant changes
motion, density or appearance. In the component panel, Edit opens the internal
graph; Detach creates an independent copy for the selected instance.

| Component | Output and useful controls | Variant |
| --- | --- | --- |
| Gradient medallion / 渐变圆环 | Transparent rotating ring; two colors, scale and rotation speed | Aurora medallion |
| Two-light sculpture / 双光雕塑 | Two lit ellipsoids on transparency; dimensions, colors, roughness, light strength and rotation | Soft twin sculpture |
| Particle fountain / 粒子喷泉 | Transparent particle layer; rate, lifetime, speed, gravity, colors; emission/burst inputs | Gentle fountain |
| Physics pile / 物理堆叠 | Falling bodies accumulate; rate, lifetime, restitution, friction and gravity | Bouncing blocks |
| Physics rain / 物理雨滴 | Falling particles with physics; rate, speed, lifetime and gravity | Slow rainfall |
| Point lattice / 点阵 | Rotating/pulsing transparent grid; rows, columns, size, color and motion | Sparse lights |
| Pulse card / 脉冲卡片 | Animated card with its background; pulse depth/rate, local clock, colors and exposure | Gentle breathing |
| Rotating cube / 旋转立方体 | Transparent 3D layer; color, size, tilt and rotation | Slow tumble |

These first eight entries reuse project-owned template graphs and existing
operators. Four existing components are extracted directly; the medallion,
lattice and 3D entries exclude the template background/compositor so they work
as layers. Source definitions and layouts live in each entry directory.

The two-node project in each entry is an executable validation harness, using
the existing project codec. The library inserts only the component, its embedded
definition closure and internal layout. It does not replace the current graph.
An existing edited definition with the same identity is preserved and reports a
conflict. Add the project-library version or detach its existing instances before
adopting a different definition. Compatible-version migration remains pending.

Physics/particles require time to emit and simulate. They do not support arbitrary
analytic seeking. A meaningful default is not a claim that all effects contain
geometry at time zero. Windows runtime and playback checks apply; Android
compilation is established, new actual device acceptance is outstanding.

This is eight semantic nodes and 16 component presets, alongside 83 operator
presets and 22 complete-project templates. It does not meet the final 40/120/24
catalog targets, and thumbnails, richer help and quality variants remain pending.
