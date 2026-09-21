#pragma once

namespace Cave::Entity {

    /**
     * @enum Type
     * @brief Represents a type of entity.
     */
    enum class Type {
        NoType,
        Dirt,
        Diamond,
        FragileDiamond,
        BreakingFragileDiamond,
        HollowDiamond,
        TimeBomb,
        Ruby,
        Ore,
        OreTransformation,
        Eater,
        Protozo,
        Cilia,
        Aggressor,
        CaveGull,
        CaveGullExplosion,
        Spinner,
        BoulderEater,
        Tetrapus,
        Binocule,
        Creep,
        Sludg,
        SaturatedSludg,
        Glutton,
        Pyram,
        Puffer,
        PufferBody,
        Blob,
        Portal,
        Mole,
        Fan,
        God,
        Charger,
        ChargerBody,
        Well,
        Chum,
        GallopQueen,
        GallopEgg,
        GallopEggPop,
        Gallop,
        PegulNormo,
        PegulFatto,
        PegulTallo,
        PegulBieye,
        PegulTrieye,
        Fusion1,
        Fusion2,
        Fusion3,
        Fusion4,
        Fusion5,
        Gate,
        Boulder,
        MagicBoulder,
        Wall,
        HorizontalWall,
        HorizontalWallPlaceholder,
        VerticalWall,
        VerticalWallPlaceholder,
        MagicWallInactive,
        MagicWallActive,
        MagicWallUsed,
        SolidWall,
        SolidTubeLeft,
        TubeLeft,
        SolidTubeRight,
        TubeRight,
        SolidTubeDown,
        TubeDown,
        SolidTubeUp,
        TubeUp,
        SolidTubeHorizontal,
        TubeHorizontal,
        SolidTubeVertical,
        TubeVertical,
        TubeCross,
        Explosion,
        TNT,
        Bomb,
        Amoeba,
        Plasma,
        StartDoor,
        StartDoorOpen,
        Jim,
        ExitDoor,
        ExitDoorOpen,
        ExitDoorOpening,
        ExitDoorComplete,
        ExitDoorFinished,
        Detonator,
        DetonatorTriggered,
        DetonatorUsed,
        Space,
    };

    inline bool isDirtLike(Type type) {
        return type == Type::Dirt || type == Type::Chum;
    }

    inline bool isPegul(Type type) {
        switch (type) {
        case Type::PegulNormo:
        case Type::PegulFatto:
        case Type::PegulTallo:
        case Type::PegulBieye:
        case Type::PegulTrieye:
            return true;
        default:
            return false;
        }
    }

    inline bool isFusion(Type type) {
        switch (type) {
        case Type::Fusion1:
        case Type::Fusion2:
        case Type::Fusion3:
        case Type::Fusion4:
        case Type::Fusion5:
            return true;
        default:
            return false;
        }
    }

    inline bool isGate(Type type) {
        return type == Type::Gate;
    }
}