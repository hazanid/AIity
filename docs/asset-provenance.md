# Asset provenance

The foundation commits no third-party art and no binary content.

The starter valley is created at runtime from Unreal Engine content:

- `/Engine/BasicShapes/Cube`
- `/Engine/BasicShapes/Cylinder`
- `/Engine/BasicShapes/Sphere`
- each shape's bundled engine material
- Unreal Directional Light and Sky Light actors

These files are part of the installed Unreal Engine and are referenced, not copied into
this repository. The game mode retains hard references to all three meshes, and packaging
config explicitly cooks `/Engine/BasicShapes`. Startup stops with a visible error if a
required mesh or valley shape cannot load or spawn. Packaged-cook proof is still unrun.
Use is covered by the Unreal Engine license that applies to the installed engine.

Founders use simple geometric bodies made from the same engine shapes. They are readable placeholders, not licensed human character art. The river is a colored non-colliding shape, not a water simulation.

No model weights are present. The foundation does not use Ollama or an LLM.

Future `.uasset` and `.umap` files are marked for Git LFS. Check the remote's LFS support and quota before adding binary content.
