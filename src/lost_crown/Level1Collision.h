/**
 * Level1Collision.h
 *
 * Collision geometry and terrain collision detection for Lost Crown Level 1.
 * Authoritative geometry matching the visual layout of assets/level1_ref.png.
 */
#pragma once

#include <Arduino.h>

namespace LostCrownLevel1 {

  // World dimensions
  static constexpr float LEVEL1_WORLD_WIDTH   = 512.0f;
  static constexpr float LEVEL1_WORLD_HEIGHT  = 240.0f;

  // Horizontal boundaries for Veera (sprite width = 36)
  static constexpr float LEVEL1_MIN_PLAYER_X  = 0.0f;
  static constexpr float LEVEL1_MAX_PLAYER_X  = 476.0f; // 512 - 36

  // Default world ground level (feet anchor)
  static constexpr float LEVEL1_DEFAULT_GROUND_Y = 214.0f;

  // Platform definition (horizontal surface)
  struct Platform {
    float x1;
    float x2;
    float y;
  };

  // Stair definition (sloped surface connecting two heights)
  struct Stair {
    float x1;
    float y1;
    float x2;
    float y2;
  };

  // Playable horizontal platforms matching level1_ref.png architecture
  static const Platform LEVEL1_PLATFORMS[] = {
    // 0: Full ground level
    {   0.0f, 512.0f, 214.0f },
    // 1: Left low platform on stilts
    {  80.0f, 126.0f, 163.0f },
    // 2: Low stone wall terrace
    { 132.0f, 206.0f, 180.0f },
    // 3: Tower lower roof (above small arch)
    { 138.0f, 197.0f, 110.0f },
    // 4: Central building main terrace (with red banner)
    { 188.0f, 308.0f, 137.0f },
    // 5: Second floor terrace (rooftop balcony with greenery)
    { 208.0f, 286.0f,  86.0f },
    // 6: Mid-stair landing / archway platform
    { 315.0f, 375.0f, 112.0f },
    // 7: Top roof terrace (under wooden gazebo / red banners)
    { 348.0f, 450.0f,  54.0f },
    // 8: Right balcony / bridge over archway
    { 370.0f, 474.0f, 124.0f },
    // 9: Far right high bridge
    { 460.0f, 512.0f,  60.0f }
  };
  static constexpr size_t LEVEL1_PLATFORM_COUNT = sizeof(LEVEL1_PLATFORMS) / sizeof(LEVEL1_PLATFORMS[0]);

  // Playable staircases connecting platform tiers
  static const Stair LEVEL1_STAIRS[] = {
    // Stair 1: Ground to Low Stone Wall
    {  96.0f, 214.0f, 132.0f, 180.0f },
    // Stair 2: Low Stone Wall to Main Terrace
    { 144.0f, 180.0f, 188.0f, 137.0f },
    // Stair 3: Main Terrace to Mid Landing
    { 286.0f, 137.0f, 315.0f, 112.0f },
    // Stair 4: Mid Landing to Top Roof
    { 315.0f, 112.0f, 348.0f,  54.0f },
    // Stair 5: Right Balcony to Ground (behind urns)
    { 412.0f, 124.0f, 452.0f, 214.0f }
  };
  static constexpr size_t LEVEL1_STAIR_COUNT = sizeof(LEVEL1_STAIRS) / sizeof(LEVEL1_STAIRS[0]);

  /**
   * Finds the closest walkable floor height at worldX beneath feetY.
   * Tolerates a small vertical snap distance for smooth walking and stair climbing.
   */
  inline float getFloorHeight(float worldX, float feetY) {
    float bestY = LEVEL1_DEFAULT_GROUND_Y;
    float minPositiveDist = 999.0f;
    static constexpr float SNAP_TOLERANCE = 8.0f;

    // Check horizontal platforms
    for (size_t i = 0; i < LEVEL1_PLATFORM_COUNT; i++) {
      const Platform &p = LEVEL1_PLATFORMS[i];
      if (worldX >= p.x1 && worldX <= p.x2) {
        float dist = p.y - feetY;
        // Surface is at or below feet (within snap tolerance)
        if (dist >= -SNAP_TOLERANCE && dist < minPositiveDist) {
          minPositiveDist = dist;
          bestY = p.y;
        }
      }
    }

    // Check stairs (slopes)
    for (size_t i = 0; i < LEVEL1_STAIR_COUNT; i++) {
      const Stair &s = LEVEL1_STAIRS[i];
      float sxMin = (s.x1 < s.x2) ? s.x1 : s.x2;
      float sxMax = (s.x1 > s.x2) ? s.x1 : s.x2;
      if (worldX >= sxMin && worldX <= sxMax) {
        float sy = s.y1 + (worldX - s.x1) * (s.y2 - s.y1) / (s.x2 - s.x1);
        float dist = sy - feetY;
        if (dist >= -SNAP_TOLERANCE && dist < minPositiveDist) {
          minPositiveDist = dist;
          bestY = sy;
        }
      }
    }

    return bestY;
  }

} // namespace LostCrownLevel1
