

#ifndef WTFILEUTILS_GENERALOBJECT_H
#define WTFILEUTILS_GENERALOBJECT_H
#include "mpi.h"
#include "math/dag_Point2.h"
#include "math/dag_Point3.h"
#include <vector>
namespace MPI_PACKETS {
  enum MainEnum : uint16_t {
    SevereDamage = 0xf157,
    CriticalDamage = 0xf056,
    Kill = 0xf058,
    Awards = 0xf078,
    Action = 0xf028,
    Replication = 0xd039,
    ReflectionNoDecompress = 0xf0aa,
    Reflection1 = 0xf02d,
    Reflection2 = 0xd136,
    ACTUALLY_NOT_REFLECTION = 0xd137,
    Tank1 = 0xf073,
    Tank2 = 0xf074,
    Rocket1 = 0xf11a,
    Rocket2 = 0xf0db,
    UnitCamera = 0xf0cc,
    // shots and hits
    GmDoStartFire = 0xf01b,
    GmDoStopFire = 0xf01c,
    UnitSingleShot = 0xf0b1,
    UnitHitEffects = 0xf0e9,
    UnitOnEffectiveHit = 0xf117,
    UnitOnEffectiveCritHit = 0xf13c,
    UnitOnHit = 0xf144,
    UnitHitAnalysis = 0xf15f, // engine class name not identified

    UnitOnExplosion = 0xf133,
    UnitLastEffectiveHit = 0xf0c2,
    UnitBulletRearm = 0xf0bd
  };
}
namespace mpi {

  struct GeneralObject : public IObject {
    GeneralObject(ParserState *state) : IObject(state, 0x5802) {}


    Message *dispatchMpiMessage(MessageID mid) override;
    void applyMpiMessage(const Message *m) override;
    ~GeneralObject() override = default;
  };

  class BSMessage : public Message {
  public:
    BSMessage(IObject *o, MessageID mid) : Message(o, mid) {}

    BitStream data{};
    bool readPayload(ParserState *state) override { return this->payload.Read(data); };
    void writePayload() override { this->payload.Write(data); };
  };

  class TankMessage : public Message {
  public:
    BitStream data{};
    TankMessage(IObject *o, MessageID mid) : Message(o, mid) {}
    bool readPayload(ParserState *state) override;
    void writePayload() override;
  };

  class CameraStateMessage : public Message {
  public:
    Quat camera_circle_quat{}; // where camera is pointing
    Point3 camera_offset{}; // camera offset from commander hatch or smg
    Point3 gun_circle_norm_vector{}; // normalized vector of where the gun circle is pointing
    uint8_t some_val{};
    float some_magnitude{};
    bool tracking_weapon{};
    ecs::EntityId weapon_eid{};
    CameraStateMessage(IObject * o) : Message(o, MPI_PACKETS::UnitCamera) {}
    bool readPayload(ParserState *state) override;
  };

  class IBattleMessage : public Message {
  protected:
    bool readPayload(ParserState *state) override;

  public:
    uint32_t time_ms{};
    IBattleMessage(IObject *o, MessageID mid) : Message(o, mid) {}
    // virtual std::string read_basic_message() = 0;
    // virtual std::string read_extended_message() = 0;
  };
  enum WeaponType : uint8_t {
    IsBullet = 1, // I presume, this is the only  case not covered
    IsBomb = 2,
    IsRocket = 3,
    iSTorpedo = 4,
  };

  enum SomeWeaponFlags : uint8_t {
    isArtillery = 1 << 0, // if 1, then this is artillery instead of bullet / shell
    isDepthBomb = 1 << 1, // if this is true, then it's a mine
    isMine = 1 << 2, // if this is true, then it's a mine
    someFlag = 1 << 5,
  };
  enum DeathType : int32_t {
    Normal = 0,
    IsCrash = -1,
    isLeave = -2,
  };

  class KillMessage : public IBattleMessage {
    bool readPayload(ParserState *state) override;

  public:
    KillMessage(IObject *o) : IBattleMessage(o, MPI_PACKETS::Kill) {}
    std::string offender_vehicle; // case 2
    std::string used_weapon; // case 0xa
    std::string destroyed_weapon; // case 0xc
    unit::Unit *offended_unit = nullptr; // case 3
    Point3 offended_unit_position{};
    unit::Unit *offender_unit = nullptr; // case 4
    Point3 offender_unit_position{};
    int DeathType{}; // case 0xb
    int offender_pid{}; // case 1
    int VictimPid{}; // only filled when a weapon is destroyed
    WeaponType some_enum{};
    uint8_t unitType{};
    bool maybe_is_burav_kill{}; // bit
    uint8_t some_weap_flags{}; // according to blk
    std::string weird_str_1{};
    std::string weird_str_2{};
    uint32_t weird_val_3{};
    std::string weird_str_4{};
  };

  class CriticalDamageMessage : public IBattleMessage {
    bool readPayload(ParserState *state) override;

  public:
    CriticalDamageMessage(IObject *o) : IBattleMessage(o, MPI_PACKETS::CriticalDamage) {}

    unit::Unit *offended_unit = nullptr;
    int player_pid{};
    std::string vehicle;
    unit::Unit *offender_unit = nullptr;
    uint8_t is_fire{}; // when doesn't equal 0, still an u8 for whatever reason
    uint8_t unitType{}; // enum value
  };

  class SevereDamageMessage : public IBattleMessage {
    bool readPayload(ParserState *state) override;

  public:
    SevereDamageMessage(IObject *o) : IBattleMessage(o, MPI_PACKETS::SevereDamage) {}
    unit::Unit *offended_unit = nullptr;
    int player_pid{};
    std::string vehicle;
    unit::Unit *offender_unit = nullptr;
    uint8_t unitType{}; // enum value
  };

  class AwardMessage : public IBattleMessage {
    bool readPayload(ParserState *state) override;

  public:
    AwardMessage(IObject *o) : IBattleMessage(o, MPI_PACKETS::Awards) {}

    int player_pid{};
    std::string award{};
    uint32_t stage{};
    uint32_t wp{};
    uint32_t exp{};
  };

  // hits

  /// 48 bit projectile id, called OffenderData in the engine:
  /// {u16 offender_oid; u8 generation; u16 index; u8 = 0}.
  /// Shared by 0xF0E9, 0xF117, 0xF13C, 0xF133, 0xF14B and ECS a9779d59, so it joins
  /// the sections. The index alone is not unique: 52 index values on one test replay
  /// are reused with a second generation, so both halves take part in equality.
  /// (index << 8 | generation) is the projectileUid written by 0xF15F.
  struct ProjectileKey {
    uint16_t offender_oid = 0; ///< uid | (type << 11): 0 aircraft (FMW), 1 ground (GM)
    uint16_t projectile_id = 0; ///< projectile entity index
    uint8_t generation = 0; ///< entity generation; 0xFF together with index 0xFFFF means none
    uint8_t tail = 0; ///< trailing byte, 0 for a real projectile

    [[nodiscard]] uint16_t offender_uid() const { return offender_oid & 0x7FF; }
    [[nodiscard]] uint8_t offender_type() const { return offender_oid >> 11; }
    /// Same value 0xF15F carries as projectileUid.
    [[nodiscard]] uint32_t projectile_uid() const {
      return (uint32_t(projectile_id) << 8) | generation;
    }
    [[nodiscard]] bool valid() const { return projectile_id != 0xFFFF || generation != 0xFF; }
    bool operator==(const ProjectileKey &o) const {
      return offender_oid == o.offender_oid && projectile_id == o.projectile_id &&
             generation == o.generation;
    }
    bool read(const BitStream *bs);
  };

  /// 0xF133 UnitOnExplosion: a projectile went off next to this unit. Carries the
  /// projectile id and two flags whose meaning is not established, so only the id
  /// is published. Fires for blasts that never produced a direct hit as well.
  struct HitExplosion {
    uint32_t time_ms = 0;
    ProjectileKey projectile{};
    unit::Unit *offended_unit = nullptr;
    unit::Unit *offender_unit = nullptr;
  };

  /// 0xF0E9 UnitHitEffects, one per hit including harmless ones.
  struct HitEffect {
    uint32_t time_ms = 0;
    ProjectileKey projectile{};
    unit::Unit *offended_unit = nullptr; ///< victim, the message recipient
    unit::Unit *offender_unit = nullptr; ///< resolved from the key uid at hit time
    Point3 local_pos{}; ///< hit point in victim local space, m
    Point3 local_dir{}; ///< travel direction in victim local space, unit length
    bool complete = false; ///< body consumed with no leftover
    // The rest of the body is read to reach the end but not published: a second
    // direction, three flags, four quantized scalars and four varints, none of
    // which have an established meaning.
  };

  /// 0xF15F, the hit camera record. Byte aligned raw float32. Not sent for every
  /// hit, and the server emits two identical copies of each record in a row.
  struct HitAnalysis {
    uint32_t time_ms = 0;
    uint16_t offender_oid = 0;
    unit::Unit *offended_unit = nullptr;
    unit::Unit *offender_unit = nullptr;

    uint16_t version = 0; ///< 9 in every record observed
    float time_s = 0.f; ///< matches the packet stamp within 1 ms
    uint32_t projectile_type = 0; ///< shell index (LEB128), not a name
    int32_t projectile_uid = 0; ///< projectile entity ext uid, low byte is the generation
    Point3 pos{}; ///< world hit point
    Point3 dir{}; ///< world travel direction, unit length
    Point3 local_pos{}; ///< hit point in victim local space
    Point3 local_dir{}; ///< direction in victim local space
    float speed = 0.f; ///< speed at impact, m/s
    float travel_distance = 0.f; ///< distance travelled, m
    int32_t seed = 0; ///< fragment damage roll seed
    // Everything past the seed is not decoded: the fields there are variable width,
    // so nothing after it can be addressed by a fixed offset.
  };

  /// UnitOnEffectiveHit (0xF117) and UnitOnEffectiveCritHit (0xF13C). Trails its
  /// hit by up to about a second, so it is joined by the projectile id, not by time.
  ///
  /// The crit here is the damage model one, not the critical hit line of the kill
  /// feed: measured against TextCriticalHitReport it lines up on 0 of 12 records on
  /// one test replay and 7 of 87 on the other.
  struct HitDamage {
    uint32_t time_ms = 0;
    ProjectileKey projectile{};
    unit::Unit *offended_unit = nullptr;
    unit::Unit *offender_unit = nullptr;
    bool crit = false; ///< came as UnitOnEffectiveCritHit rather than UnitOnEffectiveHit
    uint8_t damage_class = 0; ///< magnitude class; the high nibble scales amount
    float amount = 0.f;
  };

  /// 0xF144 UnitOnHit. Carries no projectile id, only the victim and the time.
  struct HitDirection {
    uint32_t time_ms = 0;
    unit::Unit *offended_unit = nullptr;
    Point3 world_dir{}; ///< world travel direction, unit length
    // Field 4 is a flag byte with 14 observed values; not published, meaning unknown.
  };

  /// What an effective hit did, decoded from the dm::HitVisualization blob of
  /// UnitLastEffectiveHit. The game builds its own hit camera caption the same way:
  /// the outcome is not a stored value, it follows from which event lists are filled.
  ///
  /// Only the lists up to FireSpawn are read. Everything after them is fragment
  /// geometry, which nothing here needs, and its record layout in this game version
  /// differs from the one the dev build decompiles to.
  struct HitOutcome {
    uint32_t time_ms = 0;
    uint16_t offender_oid = 0; ///< uid | (type << 11) of the shooter
    uint32_t projectile_uid = 0; ///< same value the projectile id packs as index << 8 | generation
    unit::Unit *offended_unit = nullptr;
    unit::Unit *offender_unit = nullptr;

    // The lists as they came. The outcome the game shows is not a stored value, it
    // follows from which of these are filled, but that call is left to the caller.
    std::vector<int32_t> kinetic_parts{}; ///< KineticHit: parts the round went through
    std::vector<int32_t> cumulative_parts{}; ///< CumulativeHit: parts the jet went through
    std::vector<int32_t> ricochet_parts{}; ///< Ricochet: parts the round bounced off
    std::vector<int32_t> fire_parts{}; ///< FireSpawn: parts a fire started on
    uint32_t part_count = 0; ///< parts in the victim damage model; constant per vehicle
    /// Ids of the parts whose state the server serialized with this hit. The state
    /// fields themselves have no established meaning and are not published.
    std::vector<uint16_t> changed_parts{};
    bool complete = false; ///< every list was read without running out of data
  };

  /// 0xF0BD UnitBulletRearm: rounds left in one barrel. A step of one down means a
  /// round left that barrel, which is the only per round signal automatic weapons
  /// have: they get no shot event, only a trigger down and up.
  struct AmmoEvent {
    uint32_t time_ms = 0;
    unit::Unit *unit = nullptr;
    uint8_t barrel = 0; ///< index into Unit.actual_weapons
    uint8_t slot = 0; ///< index into Unit.weapons, the belt or shell in use
    uint32_t rounds_left = 0;
  };

  enum ShotKind : uint8_t {
    ShotSingle = 0, ///< 0xF0B1, one round of a single shot gun
    ShotFireStart = 1, ///< 0xF01B, trigger pressed
    ShotFireStop = 2, ///< 0xF01C, trigger released
  };

  /// A shot or trigger event. Burst length is the gap between a start / stop pair
  /// with the same weapon_id on the same unit.
  struct ShotEvent {
    uint32_t time_ms = 0;
    unit::Unit *unit = nullptr;
    ShotKind kind = ShotSingle;
    int16_t weapon_id = -1; ///< same id as unit.Weapon.weapon_id; -1 for a single shot
    // GmDoStartFire carries a second byte; not published, meaning unknown.
  };

  class UnitHitEffectsMessage : public Message {
    bool readPayload(ParserState *state) override;

  public:
    UnitHitEffectsMessage(IObject *o) : Message(o, MPI_PACKETS::UnitHitEffects) {}
    HitEffect hit{};
  };

  class UnitHitAnalysisMessage : public Message {
    bool readPayload(ParserState *state) override;

  public:
    UnitHitAnalysisMessage(IObject *o) : Message(o, MPI_PACKETS::UnitHitAnalysis) {}
    HitAnalysis analysis{};
  };

  class UnitOnEffectiveHitMessage : public Message {
    bool readPayload(ParserState *state) override;

  public:
    UnitOnEffectiveHitMessage(IObject *o, MessageID mid) : Message(o, mid) {}
    HitDamage damage{};
  };

  class UnitOnHitMessage : public Message {
    bool readPayload(ParserState *state) override;

  public:
    UnitOnHitMessage(IObject *o) : Message(o, MPI_PACKETS::UnitOnHit) {}
    HitDirection direction{};
  };

  class UnitOnExplosionMessage : public Message {
    bool readPayload(ParserState *state) override;

  public:
    UnitOnExplosionMessage(IObject *o) : Message(o, MPI_PACKETS::UnitOnExplosion) {}
    HitExplosion explosion{};
  };

  class UnitLastEffectiveHitMessage : public Message {
    bool readPayload(ParserState *state) override;

  public:
    UnitLastEffectiveHitMessage(IObject *o) : Message(o, MPI_PACKETS::UnitLastEffectiveHit) {}
    HitOutcome outcome{};
  };

  class UnitBulletRearmMessage : public Message {
    bool readPayload(ParserState *state) override;

  public:
    UnitBulletRearmMessage(IObject *o) : Message(o, MPI_PACKETS::UnitBulletRearm) {}
    AmmoEvent ammo{};
  };

  class UnitShotMessage : public Message {
    bool readPayload(ParserState *state) override;

  public:
    UnitShotMessage(IObject *o, MessageID mid) : Message(o, mid) {}
    ShotEvent shot{};
  };
} // namespace mpi


#endif // WTFILEUTILS_GENERALOBJECT_H
