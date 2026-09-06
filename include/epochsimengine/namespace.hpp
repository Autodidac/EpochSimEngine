#pragma once

// Preserve existing symbol names and exact type identity while exposing the
// canonical library namespace. New consumers should use epochsimengine::.
namespace sandhybrid {}
namespace epochsimengine = sandhybrid;
