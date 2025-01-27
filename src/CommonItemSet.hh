#pragma once

#include <phosg/Encoding.hh>

#include "GSLArchive.hh"
#include "StaticGameData.hh"
#include "Text.hh"


class CommonItemSet {
public:
  template <typename IntT>
  struct Range {
    IntT min;
    IntT max;
  } __attribute__((packed));

  // The Table structure below describes the format of a single ItemPT entry
  // (which corresponds to a single difficulty, episode, section ID, and
  // challenge flag). ItemPT entries (within ItemPT.gsl) are named like this:
  //   string_printf("ItemPT%s%s%c%1d.rel",
  //       (is_challenge ? "c" : ""),
  //       (is_ep2 ? "l" : ""),
  //       char_for_difficulty(difficulty), // One of "nhvu"
  //       section_id);
  // For GC, use be_uint16_t/be_uint32_t; for other platforms use le variants
  template <typename U16T, typename U32T>
  struct Table {
    // This data structure uses index probability tables in multiple places. An
    // index probability table is a table where each entry holds the probability
    // that that entry's index is used. For example, if the armor slot count
    // probability table contains [77, 17, 5, 1, 0], this means there is a 77%
    // chance of no slots, 17% chance of 1 slot, 5% chance of 2 slots, 1% chance
    // of 3 slots, and no chance of 4 slots. The values in index probability
    // tables do not have to add up to 100; the game sums all of them and
    // chooses a random number less than that maximum.

    // The area (floor) number is used in many places as well. Unlike the normal
    // area numbers, which start with Pioneer 2, the area numbers in this
    // structure start with Forest 1, and boss areas are treated as the first
    // area of the next section (so De Rol Le has Mines 1 drops, for example).
    // Final boss areas are treated as the last non-boss area (so Dark Falz
    // boxes are like Ruins 3 boxes). We refer to these adjusted area numbers as
    // (area - 1).

    // This index probability table determines the types of non-rare weapons.
    // The indexes in this table correspond to the non-rare weapon types 01
    // through 0C (Saber through Wand).
    /* 0000 */ parray<uint8_t, 0x0C> base_weapon_type_prob_table;

    // This table specifies the base subtype for each weapon type. Negative
    // values here mean that the weapon cannot be found in the first N areas (so
    // -2, for example, means that the weapon never appears in Forest 1 or 2 at
    // all). Nonnegative values here mean the subtype can be found in all areas,
    // and specify the base subtype (usually in the range [0, 4]). The subtype
    // of weapon that actually appears depends on this value and a value from
    // the following table.
    /* 000C */ parray<int8_t, 0x0C> subtype_base_table;

    // This table specifies how many areas each weapon subtype appears in. For
    // example, if Sword (subtype 02, which is index 1 in this table and the
    // table above) has a subtype base of -2 and a subtype area lneght of 4,
    // then Sword items can be found when area - 1 is 2, 3, 4, or 5 (Cave 1
    // through Mine 1), and Gigush (the next sword subtype) can be found in Mine
    // 1 through Ruins 3.
    /* 0018 */ parray<uint8_t, 0x0C> subtype_area_length_table;

    // This index probability table specifies how likely each possible grind
    // value is. The table is indexed as [grind][subtype_area_index], where the
    // subtype area index is how many areas the player is beyond the first area
    // in which the subtype can first be found (clamped to [0, 3]). To continue
    // the example above, in Cave 3, subtype_area_index would be 2, since Swords
    // can first be found two areas earlier in Cave 1.
    // For example, this table could look like this:
    //   [64 1E 19 14] // Chance of getting a grind +0
    //   [00 1E 17 0F] // Chance of getting a grind +1
    //   [00 14 14 0E] // Chance of getting a grind +2
    //    ...
    //    C1 C2 C3 M1  // (Episode 1 area values from the example for reference)
    /* 0024 */ parray<parray<uint8_t, 4>, 9> grind_prob_tables;

    // This array specifies the chance that a rare weapon will have each
    // possible bonus value. This is indexed as [(bonus_value - 10 / 5)][spec],
    // so the first row refers the probability of getting a -10% bonus, the next
    // row is the chance of getting -5%, etc., all the way up to +100%. For
    // non-rare items, spec is determined randomly based on the following field;
    // for rare items, spec is always 5.
    /* 0048 */ parray<parray<U16T, 6>, 0x17> bonus_value_prob_tables;

    // This array specifies the value of spec to be used in the above lookup for
    // non-rare items. This is NOT an index probability table; this is a direct
    // lookup with indexes [bonus_index][area - 1]. A value of 0xFF in any byte
    // of this array prevents any weapon from having a bonus in that slot.
    // For example, the array might look like this:
    //   [00 00 00 01 01 01 01 02 02 02]
    //   [FF FF FF 00 00 00 01 01 01 01]
    //   [FF FF FF FF FF FF FF FF FF 00]
    //    F1 F2 C1 C2 C3 M1 M2 R1 R2 R3  // (Episode 1 areas, for reference)
    // In this example, spec is 0, 1, or 2 in all cases where a weapon can have
    // a bonus. In Forest 1 and 2 and Cave 1, weapons may have at most one
    // bonus; in all other areas except Ruins 3, they can have at most two
    // bonuses, and in Ruins 3, they can have up to three bonuses.
    /* 015C */ parray<parray<uint8_t, 10>, 3> nonrare_bonus_prob_spec;

    // This array specifies the chance that a weapon will have each bonus type.
    // The table is indexed as [bonus_type][area - 1] for non-rare items; for
    // rare items, a random value in the range [0, 9] is used instead of
    // (area - 1).
    // For example, the table might look like this:
    //   [46 46 3F 3E 3E 3D 3C 3C 3A 3A] // Chance of getting no bonus
    //   [14 14 0A 0A 09 02 02 04 05 05] // Chance of getting Native bonus
    //   [0A 0A 12 11 11 09 09 08 08 08] // Chance of getting A.Beast bonus
    //   [00 00 09 0A 0B 13 12 08 09 09] // Chance of getting Machine bonus
    //   [00 00 00 01 01 08 0A 13 13 13] // Chance of getting Dark bonus
    //   [00 00 00 00 00 01 01 01 01 01] // Chance of getting Hit bonus
    //    F1 F2 C1 C2 C3 M1 M2 R1 R2 R3  // (Episode 1 areas, for reference)
    /* 017A */ parray<parray<uint8_t, 10>, 6> bonus_type_prob_tables;

    // This array (indexed by area - 1) specifies a multiplier of used in
    // special ability determination. It seems this uses the star values from
    // ItemPMT, but not yet clear exactly in what way.
    // TODO: Figure out exactly what this does. Anchor: 80106FEC
    /* 01B6 */ parray<uint8_t, 0x0A> special_mult;

    // This array (indexed by area - 1) specifies the probability that any
    // non-rare weapon will have a special ability.
    /* 01C0 */ parray<uint8_t, 0x0A> special_percent;

    // TODO: Figure out exactly how this table is used. Anchor: 80106D34
    /* 01CA */ parray<uint8_t, 0x05> armor_shield_type_index_prob_table;

    // This index probability table specifies how common each possible slot
    // count is for armor drops.
    /* 01CF */ parray<uint8_t, 0x05> armor_slot_count_prob_table;

    // These values specify maximum indexes into another array which is
    // generated at runtime. The values here are multiplied by a random float in
    // the range [0, n] to look up the value in the secondary array, which is
    // what ends up determining the unit type.
    // TODO: Figure out and document the exact logic here. Anchor: 80106364
    /* 01D4 */ parray<uint8_t, 0x0A> unit_maxes;

    // This index probability table is indexed by [tool_class][area - 1]. The
    // tool class refers to an entry in ItemPMT, which links it to the actual
    // item code.
    /* 01DE */ parray<parray<U16T, 0x0A>, 0x1C> tool_class_prob_table;

    // This index probability table determines how likely each technique is to
    // appear. The table is indexed as [technique_num][area - 1].
    /* 040E */ parray<parray<uint8_t, 0x0A>, 0x13> technique_index_prob_table;

    // This table specifies the ranges for technique disk levels. The table is
    // indexed as [technique_num][area - 1]. If either min or max in the range
    // is 0xFF, or if max < min, technique disks are not dropped for that
    // technique and area pair.
    /* 04CC */ parray<parray<Range<uint8_t>, 0x0A>, 0x13> technique_level_ranges;

    // Each byte in this table (indexed by enemy_type) represents the percent
    // chance that the enemy drops anything at all. (This check is done after
    // the rare drop check, so it only applies to non-rare items.)
    /* 0648 */ parray<uint8_t, 0x64> enemy_type_drop_probs;

    // This array (indexed by enemy_id) specifies the range of meseta values
    // that each enemy can drop.
    /* 06AC */ parray<Range<U16T>, 0x64> enemy_meseta_ranges;

    // Each byte in this table (indexed by enemy_type) represents the class of
    // item that the enemy can drop. The values are:
    // 00 = weapon
    // 01 = armor
    // 02 = shield
    // 03 = unit
    // 04 = tool
    // 05 = meseta
    // Anything else = no item
    /* 083C */ parray<uint8_t, 0x64> enemy_item_classes;

    // This table (indexed by area - 1) specifies the ranges of meseta values
    // that can drop from boxes.
    /* 08A0 */ parray<Range<U16T>, 0x0A> box_meseta_ranges;

    // This index probability table determines which type of items drop from
    // boxes. The table is indexed as [item_class][area - 1], with item_class as
    // the result value (that is, in the example below, the game looks at a
    // single column and sums the values going down, then the chosen item class
    // is one of the row indexes based on the weight values in the column.) The
    // resulting item_class value has the same meaning as in enemy_item_classes
    // above.
    // For example, this array might look like the following:
    //   [07 07 08 08 06 07 08 09 09 0A] // Chances per area of a weapon drop
    //   [02 02 02 02 03 02 02 02 03 03] // Chances per area of an armor drop
    //   [02 02 02 02 03 02 02 02 03 03] // Chances per area of a shield drop
    //   [00 00 03 03 03 04 03 04 05 05] // Chances per area of a unit drop
    //   [11 11 12 12 12 12 12 12 12 12] // Chances per area of a tool drop
    //   [32 32 32 32 32 32 32 32 32 32] // Chances per area of a meseta drop
    //   [16 16 11 11 11 11 11 0F 0C 0B] // Chances per area of an empty box
    //    F1 F2 C1 C2 C3 M1 M2 R1 R2 R3  // (Episode 1 areas, for reference)
    /* 08C8 */ parray<parray<uint8_t, 10>, 7> box_item_class_prob_tables;

    /* 090E */ parray<uint8_t, 2> unused1;

    /* 0910 */ U32T base_weapon_type_prob_table_offset;
    /* 0914 */ U32T subtype_base_table_offset;
    /* 0918 */ U32T subtype_area_length_table_offset;
    /* 091C */ U32T grind_prob_tables_offset;
    /* 0920 */ U32T armor_shield_type_index_prob_table_offset;
    /* 0924 */ U32T armor_slot_count_prob_table_offset;
    /* 0928 */ U32T enemy_meseta_ranges_offset;
    /* 092C */ U32T enemy_type_drop_probs_offset;
    /* 0930 */ U32T enemy_item_classes_offset;
    /* 0934 */ U32T box_meseta_ranges_offset;
    /* 0938 */ U32T bonus_value_prob_tables_offset;
    /* 093C */ U32T nonrare_bonus_prob_spec_offset;
    /* 0940 */ U32T bonus_type_prob_tables_offset;
    /* 0944 */ U32T special_mult_offset;
    /* 0948 */ U32T special_percent_offset;
    /* 094C */ U32T tool_class_prob_table_offset;
    /* 0950 */ U32T technique_index_prob_table_offset;
    /* 0954 */ U32T technique_level_ranges_offset;
    /* 0958 */ uint8_t armor_or_shield_type_bias;
    /* 0959 */ parray<uint8_t, 3> unused2;
    /* 095C */ U32T unit_maxes_offset;
    /* 0960 */ U32T box_item_class_prob_tables_offset;
    /* 0964 */ U32T unused_offset2;
    /* 0968 */ U32T unused_offset3;
    /* 096C */ U32T unused_offset4;
    /* 0970 */ U32T unused_offset5;
    /* 0974 */ U32T unused_offset6;
    /* 0978 */ U32T unused_offset7;
    /* 097C */ U32T unused_offset8;
    /* 0980 */ U16T unknown_f1[0x20];
    /* 09C0 */ U32T unknown_f1_offset;
    /* 09C4 */ U32T unknown_f2[3];
    /* 09D0 */ U32T offset_table_offset;
    /* 09D4 */ U32T unknown_f3[3];
    /* 09E0 (end of structure) */

    void print(FILE* stream) const;
  } __attribute__((packed));

  CommonItemSet(std::shared_ptr<const std::string> data);

  using BETable = Table<be_uint16_t, be_uint32_t>;
  using LETable = Table<le_uint16_t, le_uint32_t>;

  const BETable& get_table(
      Episode episode, GameMode mode, uint8_t difficulty, uint8_t secid) const;

private:
  GSLArchive gsl;
};



class RELFileSet {
public:
  template <typename ValueT, typename WeightT = ValueT>
  struct WeightTableEntry {
    ValueT value;
    WeightT weight;
  } __attribute__((packed));

  using WeightTableEntry8 = WeightTableEntry<uint8_t>;
  using WeightTableEntry32 = WeightTableEntry<be_uint32_t>;

protected:
  std::shared_ptr<const string> data;
  StringReader r;

  struct TableSpec {
    be_uint32_t offset;
    uint8_t entries_per_table;
    parray<uint8_t, 3> unused;
  } __attribute__((packed));

  RELFileSet(std::shared_ptr<const std::string> data);

  template <typename T>
  std::pair<const T*, size_t> get_table(
      const TableSpec& spec, size_t index) const {
    const T* entries = &r.pget<T>(
        spec.offset + index * spec.entries_per_table * sizeof(T),
        spec.entries_per_table * sizeof(T));
    return make_pair(entries, spec.entries_per_table);
  }
};

class ArmorRandomSet : public RELFileSet {
public:
  // This class parses and accesses data from ArmorRandom.rel
  ArmorRandomSet(std::shared_ptr<const std::string> data);

  std::pair<const WeightTableEntry8*, size_t> get_armor_table(size_t index) const;
  std::pair<const WeightTableEntry8*, size_t> get_shield_table(size_t index) const;
  std::pair<const WeightTableEntry8*, size_t> get_unit_table(size_t index) const;

private:
  const parray<TableSpec, 3>* tables;
};

class ToolRandomSet : public RELFileSet {
public:
  // This class parses and accesses data from ToolRandom.rel
  ToolRandomSet(std::shared_ptr<const std::string> data);

  struct TechDiskLevelEntry {
    enum class Mode : uint8_t {
      LEVEL_1 = 0,
      PLAYER_LEVEL_DIVISOR = 1,
      RANDOM_IN_RANGE = 2,
    };
    Mode mode;
    uint8_t player_level_divisor_or_min_level;
    uint8_t max_level;
  } __attribute__((packed));

  std::pair<const uint8_t*, size_t> get_common_recovery_table(size_t index) const;
  std::pair<const WeightTableEntry8*, size_t> get_rare_recovery_table(size_t index) const;
  std::pair<const WeightTableEntry8*, size_t> get_tech_disk_table(size_t index) const;
  std::pair<const TechDiskLevelEntry*, size_t> get_tech_disk_level_table(size_t index) const;

private:
  const TableSpec* common_recovery_table_spec;
  const TableSpec* rare_recovery_table_spec;
  const TableSpec* tech_disk_table_spec;
  const TableSpec* tech_disk_level_table_spec;
};

class WeaponRandomSet : public RELFileSet {
public:
  // This class parses and accesses data from WeaponRandom*.rel
  WeaponRandomSet(std::shared_ptr<const std::string> data);

  struct RangeTableEntry {
    be_uint32_t min;
    be_uint32_t max;
  } __attribute__((packed));

  std::pair<const WeightTableEntry8*, size_t> get_weapon_type_table(size_t index) const;
  const parray<WeightTableEntry32, 6>* get_bonus_type_table(size_t which, size_t index) const;
  const RangeTableEntry* get_bonus_range(size_t which, size_t index) const;
  const parray<WeightTableEntry32, 3>* get_special_mode_table(size_t index) const;
  const RangeTableEntry* get_standard_grind_range(size_t index) const;
  const RangeTableEntry* get_favored_grind_range(size_t index) const;

private:
  struct Offsets {
    be_uint32_t weapon_type_table; // [{c, o -> (table)}](10)
    be_uint32_t bonus_type_table1; // [[{u32 value, u32 weight}](6)](9)
    be_uint32_t bonus_type_table2; // [[{u32 value, u32 weight}](6)](9)
    be_uint32_t bonus_range_table1; // [{u32 min_index, u32 max_index}](9)
    be_uint32_t bonus_range_table2; // [{u32 min_index, u32 max_index}](9)
    be_uint32_t special_mode_table; // [[{u32 value, u32 weight}](3)](8)
    be_uint32_t standard_grind_range_table; // [{u32 min, u32 max}](6)
    be_uint32_t favored_grind_range_table; // [{u32 min, u32 max}](6)
  } __attribute__((packed));

  const Offsets* offsets;
};
