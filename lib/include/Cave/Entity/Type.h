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
        Chaos,
        ChaosExplosion,
        JimlinShipInactive,
        JimlinShipActive,
        Jimlin1,
        Jimlin2,
        Jimlin3,
        JimlinBlock,
        Jimlin4,
        JimlinKing,
        PrivateGate,
        VaultButton,
        KingShipInactive,
        KingShipActive,
        Singularity,
        Ostia,
        Murus,
        Tera,
        Vitus,
        Adama,
        Terminus,
        Initia,
        Nihilus,
        SingularityExplosion,
        JimlinDock,
        Magma,
        HotBoulder,
        HotBoulderCracked,
        Lava,
        Fire,
        Fireball,
        Pyrozo,
        PyrozoExtinguished,
        Hellgull,
        Charia,
    };

    inline bool isDirtLike(Type type) {
        return type == Type::Dirt || type == Type::Chum || type == Type::Magma;
    }

    inline bool isLavaImmune(Type type) {
        return type == Type::Pyrozo || type == Type::PyrozoExtinguished
            || type == Type::Hellgull || type == Type::Charia;
    }

    inline bool isFireImmune(Type type) {
        return type == Type::Chaos || type == Type::Pyrozo || type == Type::PyrozoExtinguished
            || type == Type::Hellgull || type == Type::Charia;
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

    inline bool isPyrozoVariant(Type type) {
        return type == Type::Pyrozo || type == Type::PyrozoExtinguished;
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

    inline bool isGateVariant(Type type) {
        return type == Type::Gate || type == Type::PrivateGate;
    }

    inline bool isJimlinBlockVariant(Type type) {
        return type == Type::JimlinBlock || type == Type::VaultButton || type == Type::JimlinDock;
    }

    inline bool isGodVariant(Type type) {
        return type == Type::God || type == Type::Chaos;
    }

    inline bool isCosmic(Type type) {
        switch (type) {
        case Type::Singularity:
        case Type::Ostia:
        case Type::Murus:
        case Type::Tera:
        case Type::Vitus:
        case Type::Adama:
        case Type::Terminus:
        case Type::Initia:
        case Type::Nihilus:
            return true;
        default:
            return false;
        }
    }

    inline bool isSpaceSlotType(Type type) {
        return type == Type::Space || isCosmic(type);
    }

    inline bool isMonster(Type type) {
        switch (type) {
        case Type::Eater:
        case Type::Protozo:
        case Type::Cilia:
        case Type::Aggressor:
        case Type::CaveGull:
        case Type::Spinner:
        case Type::BoulderEater:
        case Type::Tetrapus:
        case Type::Binocule:
        case Type::Creep:
        case Type::Sludg:
        case Type::SaturatedSludg:
        case Type::Glutton:
        case Type::Pyram:
        case Type::Puffer:
        case Type::PufferBody:
        case Type::Blob:
        case Type::Mole:
        case Type::God:
        case Type::Charger:
        case Type::ChargerBody:
        case Type::GallopQueen:
        case Type::Gallop:
        case Type::PegulNormo:
        case Type::PegulFatto:
        case Type::PegulTallo:
        case Type::PegulBieye:
        case Type::PegulTrieye:
        case Type::Fusion1:
        case Type::Fusion2:
        case Type::Fusion3:
        case Type::Fusion4:
        case Type::Fusion5:
        case Type::Chaos:
        case Type::Singularity:
        case Type::Ostia:
        case Type::Murus:
        case Type::Tera:
        case Type::Vitus:
        case Type::Adama:
        case Type::Terminus:
        case Type::Initia:
        case Type::Nihilus:
        case Type::Pyrozo:
        case Type::PyrozoExtinguished:
        case Type::Hellgull:
        case Type::Charia:
            return true;
        default:
            return false;
        }
    }

    inline bool isMagicWall(Type type) {
        switch (type) {
        case Type::MagicWallInactive:
        case Type::MagicWallActive:
        case Type::MagicWallUsed:
            return true;
        default:
            return false;
        }
    }

    inline bool isJimlinShip(Type type) {
        return type == Type::JimlinShipInactive || type == Type::JimlinShipActive
            || type == Type::KingShipInactive || type == Type::KingShipActive;
    }

    inline bool isParkedJimlinShip(Type type) {
        return type == Type::JimlinShipInactive || type == Type::KingShipInactive;
    }

    inline bool isActiveJimlinShip(Type type) {
        return type == Type::JimlinShipActive || type == Type::KingShipActive;
    }

    inline bool isJimlinShipVariant(Type type) {
        return type == Type::JimlinShipInactive || type == Type::KingShipInactive;
    }

    inline bool isJimlin(Type type) {
        return type == Type::Jimlin1 || type == Type::Jimlin2 || type == Type::Jimlin3
            || type == Type::Jimlin4 || type == Type::JimlinKing;
    }

    inline bool isDiamondTile(Type type) {
        return type == Type::Diamond
            || type == Type::FragileDiamond
            || type == Type::HollowDiamond;
    }

    inline bool isPlayer(Type type) {
        return type == Type::Jim || isActiveJimlinShip(type);
    }

    inline bool isHuntTarget(Type type) {
        return isPlayer(type) || isJimlin(type);
    }

    inline bool isExitDoor(Type type) {
        switch (type) {
        case Type::ExitDoor:
        case Type::ExitDoorOpen:
        case Type::ExitDoorOpening:
        case Type::ExitDoorComplete:
        case Type::ExitDoorFinished:
            return true;
        default:
            return false;
        }
    }
}