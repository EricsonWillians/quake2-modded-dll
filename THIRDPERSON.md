# Quake 2 Rerelease Third-Person Camera System

This document explains the implementation of the advanced third-person camera system for Quake 2 Rerelease.

## Overview

The third-person camera system provides a smooth, collision-aware view that follows the player character while maintaining proper visibility and handling animations. This creates an experience similar to modern third-person shooters while preserving Quake 2's core gameplay.

## Features

- **Smooth Camera Movement**: Camera follows player with configurable smoothing
- **Intelligent Collision Detection**: Prevents camera from clipping through walls
- **Sky Box Detection**: Prevents camera from seeing outside the level
- **Precise Aiming**: Accurate targeting system for weapons
- **Player Avatar**: Visual representation of the player character
- **Customizable Settings**: Adjustable camera distance, height, and position

## Technical Implementation

The system is comprised of several key components:

### 1. Camera System

The camera positioning logic uses a sophisticated algorithm that:

- Places the camera behind the player based on configurable distance
- Intelligently handles collisions with level geometry
- Prevents camera from showing areas outside the map
- Smoothly transitions between camera positions

```cpp
vec3_t desiredPos = playerEyePos - (forward * effective_distance) + (right * effective_side);
desiredPos.z += effective_height;

// Collision handling
HandleCameraCollision(ent, playerEyePos, desiredPos, cameraPos);

// Smooth transitions
ent->client->ps.viewoffset = LerpVec(ent->client->ps.viewoffset, targetViewOffset, effective_smooth);
```

### 2. Player Avatar System

An avatar entity represents the player visually in third-person view:

- Created as a separate entity with player model
- Syncs animations and state with the player entity
- Updates position and orientation in real-time
- Preserves player collision and physics behavior

```cpp
// Copy entity state from player
avatar->s = ent->s;
avatar->s.modelindex = 255;  // Player model index
```

### 3. Aiming System

An enhanced targeting system allows accurate aiming in third-person mode:

- Projects a trace from player's eyes to determine aim point
- Adjusts weapon fire direction to match visual crosshair
- Maintains accuracy comparable to first-person mode

```cpp
// Create vector from start position to target point
vec3_t dir = self->client->thirdperson_target - start;

// Normalize to get direction
float length = dir.length();
if (length > 0) {
    aimdir = dir * (1.0f / length);
}
```

## Configuration Variables

The following CVARs control third-person camera behavior:

| Variable | Default | Description |
|----------|---------|-------------|
| `sv_thirdperson` | 0 | Enables (1) or disables (0) third-person view |
| `tp_distance` | 64 | Camera distance from player |
| `tp_height` | 0 | Camera height offset |
| `tp_side` | 0 | Camera side offset (positive = right) |
| `tp_smooth` | 0.5 | Camera movement smoothing (0.0-1.0) |

## Usage

### Player Commands

- **thirdperson**: Toggle third-person view mode
  ```
  bind f3 "thirdperson"
  ```

### Console Variables

Set these variables in the console or autoexec.cfg:

```
sv_thirdperson 1        // Enable third-person view
tp_distance 80          // Set camera distance to 80 units
tp_height 16            // Raise camera by 16 units
tp_side 12              // Offset camera 12 units to the right
tp_smooth 0.7           // Set camera smoothing to 70%
```

## Integration with Game Code

The third-person system hooks into the Quake 2 engine at several key points:

1. **ClientBeginServerFrame**: Updates third-person view each frame
2. **ClientThink**: Processes player input and updates camera position
3. **Weapon firing functions**: Adjusts weapon aim direction in third-person

## Implementation Details

### Collision Detection

The camera employs multi-layered collision detection:

1. **Primary ray cast** from player to desired camera position
2. **Proximity probes** to detect nearby walls and adjust position
3. **Sky detection** to prevent seeing outside the level boundaries

```cpp
// Probe around the camera in a circle to detect nearby walls
for (int i = 0; i < numProbes; i++) {
    float angle = (float)i / numProbes * PI * 2.0f;
    probeOffset.x = cos(angle) * probeRadius;
    probeOffset.y = sin(angle) * probeRadius;
    probeOffset.z = 0;
    
    probeEnd = cameraPos + probeOffset;
    
    trace_t probeTrace = gi.traceline(cameraPos, probeEnd, ent, MASK_SOLID);
    if (probeTrace.fraction < 1.0f) {
        // Hit a wall, calculate adjustment
        netAdjustment = netAdjustment + 
            (probeTrace.plane.normal * (1.0f - probeTrace.fraction));
        wallHits++;
    }
}
```

### Animation Handling

The player avatar's animations are managed by:

1. Copying the player entity's frame properties
2. Preserving critical entity state while updating visuals
3. Adjusting view angles for natural-looking movement

```cpp
// Copy entity state
avatar->s = ent->s;
avatar->s.modelindex = 255;

// Set proper rotation for third-person appearance
avatar->s.angles[YAW] = ent->client->v_angle[YAW];
avatar->s.angles[PITCH] = 0;
avatar->s.angles[ROLL] = 0;
```

## Troubleshooting

Common issues and solutions:

- **Camera clipping through walls**: Increase `tp_distance` or adjust `tp_side`
- **Jerky camera movement**: Increase `tp_smooth` value
- **Animation glitches**: Ensure player model is properly set up
- **Aiming inaccuracy**: Check weapon aim adjustment code

## Performance Considerations

The third-person system is designed to be efficient:

- Avatar entity only created when needed
- Smart collision detection with limited ray casts
- Smooth interpolation without excessive calculations
- Clean cleanup during level transitions

## Further Development

Potential enhancements for future versions:

- Camera obstruction transparency
- Dynamic camera adjustment based on context
- More animation improvements
- Additional camera modes (over-shoulder, cinematic)
- Custom third-person crosshair options