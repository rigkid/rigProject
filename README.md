# rigProject

![preview](examples/document/img/preview.png)

Host **project** envelope (`CProject` / `CPage` PODs) + `.rig` **document** IO, plus Contract `rig.*` import (`ContractImport.h`). Depends on rigComponent for registration order.

## Lingo: document vs project

- **Document** = the portable `.rig` file — Contract JSON (`entities[].components` with `rig.*` schema ids). What RigViewer presents and RigPlayer plays.
- **Project** = the host-side working envelope (`CProject` / `CPage`) inside a RigKit host session. This pack owns it.

Contract JSON is the only `.rig` dialect: what this pack writes is what RigViewer reads. `tools/contract_smoke` ("save is readable by Contract import") holds that seam shut.

## Serialize (`.rig` document)

- Root shape: `{ "rig": "<contract version>", "document": {...}, "entities": [...] }`
- Entity shape: `{ "id": "e12", "components": { "rig.spatial.transform": {...} } }` — the name rides in `rig.meta.named`
- Every codec carries a schema id: `rig.*` where the Contract defines the shape, `x.<vendor>.*` for host-only data. Registration refuses a codec without one, so nothing reaches a file unlabelled.
- Skips: Selection (session), Canvas/Texture (non-portable), document metadata entity in `entities[]`
- Packs can `registerSerializer` / set root extension writer/reader for domain envelopes

### File extension

Default is **`.rig`**. Apps can switch without changing the JSON shape:

```cpp
doc->setFileExtension("rig");     // default
doc->setFileExtension("rigdoc");  // longer form
doc->requestSave(doc->documentPath(AppPaths::getDataDir() + "/show"));
doc->requestLoad(AppPaths::getDataDir() + "/show"); // appends preferred ext if missing
```

Paths that already include a suffix (`.rig`, `.rigdoc`, `.json`, etc.) are left alone.

## Contract import (`rig.*` JSON)

Reads [RigWorks](https://github.com/rigkid/RigWorks) documents from any host, including the ones this pack writes.

```cpp
#include "ContractImport.h"

auto result = rigkit::project::importContractFile(*ecs, path);
if (!result.ok) { /* result.error */ }
// Update:
rigkit::project::tickContractModulators(*ecs, result, timeSec);
```

Maps geometry / transform / camera / light / material albedo / paint / LFO+binding / **`rig.media.code` → `CCode`** into host PODs. UI panel / control / action rows stay on `ContractImportResult` for the app’s UI fulfillment (e.g. RigViewer’s Contract UI window).

## Build Example

```bash
cmake -S examples/document -B examples/document/build
cmake --build examples/document/build --target document
```

[API/docs](https://rigkid.github.io/rigProject/)
