#pragma once
#include "math/dag_Point3.h"
#include "state/StateRewinder.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <limits>
#include <vector>

struct ZSTD_CCtx_s;

namespace unit {
  struct TurretData {
    Point3 rel, abs, gun_rel, gun_abs;
    bool operator==(const TurretData &other) const {
      return rel == other.rel && abs == other.abs && gun_rel == other.gun_rel && gun_abs == other.gun_abs;
    }
  };

  using TurretClock = ObjectRewindState<uint32_t, false, false, false>;

  // Lossless angle blocks; timestamps and rewind position are shared by the vehicle.
  class TurretHistory {
  public:
    static constexpr size_t BLOCK_SAMPLES = 256;
    class Encoder {
      ZSTD_CCtx_s *context{};

    public:
      Encoder() = default;
      ~Encoder();
      Encoder(const Encoder &) = delete;
      Encoder &operator=(const Encoder &) = delete;
      size_t compress(void *destination, size_t capacity, const void *source, size_t size);
    };
    struct TimeState {
      uint32_t time_ms;
      TurretData data;
      bool operator==(const TimeState &other) const { return data == other.data; }
    };
    class Iterator {
      const TurretHistory *owner_;
      size_t index_;
      mutable size_t block_ = std::numeric_limits<size_t>::max();
      mutable std::array<TurretData, BLOCK_SAMPLES> decoded_{};

    public:
      using value_type = TimeState;
      using reference = TimeState;
      using pointer = void;
      using difference_type = std::ptrdiff_t;
      using iterator_category = std::input_iterator_tag;
      Iterator(const TurretHistory *owner, size_t index) : owner_(owner), index_(index) {}
      TimeState operator*() const;
      Iterator &operator++() {
        ++index_;
        return *this;
      }
      Iterator operator++(int) {
        auto previous = *this;
        ++*this;
        return previous;
      }
      bool operator==(const Iterator &other) const { return owner_ == other.owner_ && index_ == other.index_; }
    };
    class View {
      const TurretHistory *owner_;

    public:
      using value_type = TimeState;
      using size_type = size_t;
      explicit View(const TurretHistory *owner) : owner_(owner) {}
      size_t size() const { return owner_->size(); }
      bool empty() const { return size() == 0; }
      TimeState operator[](size_t index) const { return owner_->at(index); }
      Iterator begin() const { return {owner_, 0}; }
      Iterator end() const { return {owner_, size()}; }
    };

    explicit TurretHistory(const TurretClock &clock) :
      clock_(clock), first_frame_(clock.history().size()), view_(this) {}
    TurretHistory(const TurretHistory &) = delete;
    TurretHistory &operator=(const TurretHistory &) = delete;
    void push(const TurretData &data, Encoder &encoder);
    size_t size() const { return blocks_.size() * BLOCK_SAMPLES + tail_.size(); }
    bool hasData() const { return size() != 0; }
    const View &history() const { return view_; }
    const TurretClock &clock() const { return clock_; }
    uint32_t firstFrame() const { return first_frame_; }
    const TimeState *currState() const;
    const TurretData *curr() const { return &currState()->data; }
    TimeState at(size_t index) const;
    size_t readBlock(size_t index, TurretData *destination) const;
    size_t blockCount() const { return blocks_.size() + (tail_.empty() ? 0 : 1); }

  private:
    struct Block {
      std::vector<uint8_t> bytes;
      bool raw;
    };
    const TurretClock &clock_;
    uint32_t first_frame_;
    std::vector<Block> blocks_;
    std::vector<TurretData> tail_;
    View view_;
    mutable TimeState current_{};
    mutable size_t current_index_ = std::numeric_limits<size_t>::max();
    void flush(Encoder &encoder);
  };
} // namespace unit
