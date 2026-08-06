# Document

![preview](img/preview.png)

Cue storyboard: one `CProject`, three `CPage` records, and page cards as plain
shapes under a fit `board-root`. Active page (`activePageIndex`) gets a selection outline. 
On run, saves then reloads `<execDir>/data/show.rig`.

```bash
cmake -S . -B build
cmake --build build --target document
./build/bin/document/document
```
