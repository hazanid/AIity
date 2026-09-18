# Asset provenance

The foundation commits no third-party art and no binary content.

The starter valley is created at runtime from Unreal Engine content:

- `/Engine/BasicShapes/Cube`
- `/Engine/BasicShapes/Cylinder`
- `/Engine/BasicShapes/Sphere`
- `/Engine/BasicShapes/BasicShapeMaterial_Inst` and its `BasicShapeMaterial` parent
- Unreal Directional Light, Sky Light, and Sky Atmosphere actors

These files are part of the installed Unreal Engine and are referenced, not copied into
this repository. The game mode retains hard references to all three meshes and the tint
material; founder mesh components retain the same material. Packaging config explicitly
cooks `/Engine/BasicShapes`. The installed engine material exposes the `Color` vector
parameter; the meshes' default WorldGridMaterial does not. Startup validates the tint
parameter and stops with a visible error if a required asset or valley actor is missing.
Packaged-cook proof is still unrun.
Use is covered by the Unreal Engine license that applies to the installed engine.

Founders use simple geometric bodies made from the same engine shapes. They are readable placeholders, not licensed human character art. The river is a colored non-colliding shape, not a water simulation.

Runtime geometry and lighting use Movable components, with an atmosphere sun and one
sky capture after atmosphere creation. The sun is fixed; this is not a day/night cycle.
Software Lumen uses generated mesh distance fields; hardware ray tracing is not enabled.

No model weights are present. The foundation does not use Ollama or an LLM.

Future `.uasset` and `.umap` files are marked for Git LFS. Check the remote's LFS support and quota before adding binary content.
