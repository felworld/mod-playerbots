#ifndef PLAYERBOTS_WPVPTERRAINLOS_H
#define PLAYERBOTS_WPVPTERRAINLOS_H

class Unit;

// Terrain occlusion (Felworld): true when the terrain heightmap rises above
// the sight line between the two units. Server LOS (IsWithinLOSInMap) only
// ray-tests model geometry — vmaps and gameobjects — so hills and ridges are
// invisible to it, and bots acquired targets straight through them from up to
// WpvpVisionDistance away. This closes the gap for unprovoked target
// acquisition: a target behind a rise a human couldn't see through is not a
// target. Indoors units are exempt (WMO geometry is vmap territory, and the
// outdoor heightmap above a cave would false-positive).
bool WpvpTerrainOccludes(Unit* viewer, Unit* target);

#endif
