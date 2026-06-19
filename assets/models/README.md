# Avatar assets

Drop a high-quality skinned female avatar here and it will be picked up
automatically at startup.

## Supported formats

- **glTF** (`.glb` single-file or `.gltf` + bin/textures) — recommended.
- **FBX** (`.fbx`) — supported via Assimp.

## What to include

- A **skeleton** (armature) with bone hierarchy.
- **Skinned meshes** (per-vertex bone weights; Assimp's
  `aiProcess_LimitBoneWeights` clamps to 4 influences/vertex).
- **Animation clips** embedded in the file: at minimum an **idle** and a
  **walk** clip. A **run** clip is used when present.

## File naming

`SimAvatar::init()` searches this directory for, in order:

1. `female_avatar.glb` / `.gltf` / `.fbx`
2. `avatar.glb` / `.gltf` / `.fbx`
3. `sim.glb` / `.gltf` / `.fbx`
4. The first `.glb`/`.gltf`/`.fbx` it finds.

## Clip selection

Clip names are matched case-insensitively by substring:

- **idle**: contains `idle`, `rest`, `stand`, or `breath`
- **walk**: contains `walk` or `move`
- **run**: contains `run`, `sprint`, or `jog`

If no matching name is found, the first clip is used as idle.

## Coordinate / scale notes

- The avatar is placed at the Sim's world position (feet at y=0) and yawed
  to `facing_deg`. 0 deg faces +Z (`atan2(dir.x, dir.z)`), matching Mixamo's
  default forward. Rotate your model in Blender/Mixamo to +Z if needed.
- `u_model` does not rescale the mesh — keep the asset in meters (≈1.6–1.8 m
  tall) so it matches the 1 m floor tiles.

## Where to get an asset

Mixamo (free, Adobe account): export a character with the Idle + Walk clips
in FBX or glTF. ReadyPlayerMe also exports glTF with a full skeleton.