#pragma once
#include "Cave/Entity/Base.h"
#include "Cave/Entity/MacowEggCook.h"

namespace Cave::Entity {

    /**
     * @class MacowEggCooked
     * @brief Finished cooked egg. Never hatches. Jim can pick it up and drop it
     * like a raw Macow egg. Uses the last frame of macow_egg_cooked.png.
     */
    class MacowEggCooked : public Base {
    public:
        static constexpr int FRAME = MacowEggCook::FRAME_BASE + MacowEggCook::FRAME_COUNT - 1;

        MacowEggCooked()
            : Base(Type::MacowEggCooked, FRAME) {
            addTrait(Trait::Slippery);
            addTrait(Trait::Traversable);
        }
    };
}
