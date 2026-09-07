# Open-source reuse policy

> Decision: commercial-grade quality, open-source development and distribution.
> Updated: 2026-09-06. No project-wide LICENSE has been selected yet.

## Reuse direction

User instruction, 2026-09-07: **能参考、复制、复刻开源项目的，就直接用，
不要自己硬写。** This is the default implementation rule for the remaining
seven-step delivery, not merely permission to read upstream code.

- Before implementing a capability, inspect relevant mature open-source code
  and the project's reuse plans. Prefer a compatible existing dependency, direct
  source reuse, focused extraction or adaptation when it meets the requirement.
- Prefer `C:/source/vcpkg` for dependencies and build tools. For source extraction,
  preserve the upstream revision and notices and keep the old project and original
  GammaRay repository read-only.
- Write project-specific contracts, ownership, platform and UI adapters as needed.
  Do not reimplement an available algorithm or subsystem solely to make it our own.
- When a new implementation is necessary, record the inspected sources and the
  concrete missing behavior, platform, dependency, licensing or integration
  constraint in the relevant technical/validation document. A generic statement
  that upstream code is unsuitable is insufficient. Keep this record proportional
  to the change; do not turn it into a separate approval flow.
- Direct copying and translated/adapted implementations follow the same provenance,
  actual-license and artifact requirements below. Open-source reuse remains subject
  to the project's zero-Qt and platform boundaries.

Evaluate mature open-source implementations before rebuilding their features.
Source study, copying, modification, shader adaptation and maintained forks are
eligible approaches. Permissive and copyleft licenses are both eligible; GPL
code from Coollab or ossia score is no longer restricted to behavior-only study.
This supersedes earlier closed-source assumptions in the upgrade discussions.

Open source does not mean that all licenses can be combined or that attribution
alone is sufficient. Check the exact license and integration before adopting
code. Preserve its original copyright, notices and license text.

## License selection and artifact boundaries

The repository currently has no LICENSE file. Do not interpret this document as
a license grant or choose a project-wide license silently. Decide the outbound
license for affected artifacts before an integration that determines their
distribution terms. A GPLv3-compatible Studio license is a candidate when
adopting GPLv3 implementation; it is not selected by this document.

Track Studio, shared libraries, Player and content packages separately. A GPL
dependency in Studio is not automatically a dependency of the portable Player;
check the actual code and link graph. Conversely, importing GPL code into the
shared runtime can affect every application combining with it. Namespaces,
wrappers, DLLs and process boundaries are not automatic license exemptions.

For each candidate:

| License family | Integration decision |
| --- | --- |
| MIT/BSD/zlib/Apache | Eligible; retain required notices and examine exact compatibility, including patent terms where applicable |
| LGPL/MPL | Eligible; check library/file-level obligations and modification/relink requirements as applicable |
| GPL | Eligible; check exact version, only/or-later wording, exceptions, combined-work terms and corresponding source |
| AGPL | Eligible for evaluation; additionally assess network-use source obligations if applicable |
| Proprietary/source-available/no license | Obtain applicable permission before code reuse; public access alone is not permission |

This table is a workflow guide; the actual license governs. Build-time tools,
generated code and embedded runtime libraries have different roles, so identify
what is actually included in the artifact. Review shaders, templates, fonts,
models and media separately from the repository's top-level code license.

## Integration record

Record the following beside an imported package or maintained fork:

- upstream URL, exact revision and imported file paths;
- license identifier, version, exceptions and copyright notices;
- local modifications and upstream synchronization approach;
- destination modules and distributed artifacts;
- transitive dependencies and bundled/generated assets;
- required source, build materials, notices and redistribution conditions;
- tests for adapted behavior and supported platforms.

Release source must match the shipped binary and include corresponding source
and build materials where required. Public source hosting alone is not a
substitute for meeting each license's distribution conditions. For mobile
Player, verify the selected dependency combination against the intended signing
and store distribution terms; do not assume desktop approval covers iOS or
Android artifacts.

## Engineering constraints

### Authorized first-party reuse

The user confirms and authorizes reuse of our own GammaRayPremium foundations
under `D:/GoCloud/GammaRayPremium/src/px_deps/px_common`. Treat verified owned
files as first-party extraction/adaptation, not unlicensed external code
requiring redundant permission. Keep source revision/file hashes, copyright
notices and changes. This does not select the new repository's outbound license
or authorize importing credentials, private data or unrelated product modules.

Embedded upstream sources remain third-party. In particular, the QR wrapper and
Nayuki's MIT QR implementation have distinct provenance. Follow
`gammaray_common_reuse_plan.md`; apply the dependency/artifact checks above to
Asio, cpr, TLS providers and other transitive libraries.

### Dependency boundaries

Reuse must satisfy the zero-Qt public architecture and the Studio/Player split.
Prefer focused extraction or adaptation when upstream code is bound to Qt,
Direct3D, browser APIs or another incompatible host. Copied or translated code
retains applicable source-license obligations.

Keep third-party/native types inside adapters. Our public APIs continue to use
values, references, smart pointers and typed handles; newly authored code
follows the Google naming and four-space conventions in AGENTS.md. Imported
sources retain upstream style unless explicitly maintained and modified for a
defined feature. Maintenance responsibility never transfers copyright by itself.

For this reference set:

- TiXL, cables and Material Maker: eligible for direct reuse/adaptation after
  checking the selected source and dependencies.
- Coollab and ossia score: equally eligible for source-level study and reuse;
  evaluate exact GPL terms and the affected artifact license before integration.
- TouchDesigner: public documentation and observable behavior remain references;
  no permission to copy proprietary implementation is inferred.

No third-party implementation is imported as part of this documentation change.

## Primary guidance

- [GNU license compatibility](https://www.gnu.org/licenses/license-compatibility.en.html)
- [GNU license FAQ](https://www.gnu.org/licenses/gpl-faq.en.html)
- [GPLv3 source distribution guide](https://www.gnu.org/licenses/quick-guide-gplv3.en.html)
