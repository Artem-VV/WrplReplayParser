#include "turretHistory.h"
#include "Logger.h"
#include <cstring>
#include <zstd.h>
#include <type_traits>

static_assert(std::is_trivially_copyable_v<unit::TurretData>);

namespace unit {
  TurretHistory::Encoder::~Encoder() { ZSTD_freeCCtx(context); }

  size_t TurretHistory::Encoder::compress(void *destination, size_t capacity, const void *source, size_t size) {
    if (!context) {
      context = ZSTD_createCCtx();
      if (!context)
        EXCEPTION("Cannot allocate turret history encoder");
    }
    return ZSTD_compressCCtx(context, destination, capacity, source, size, 1);
  }

  void TurretHistory::flush(Encoder &encoder) {
    const size_t raw_size = tail_.size() * sizeof(TurretData);
    Block block;
    block.bytes.resize(ZSTD_compressBound(raw_size));
    size_t bytes = encoder.compress(block.bytes.data(), block.bytes.size(), tail_.data(), raw_size);
    if (ZSTD_isError(bytes))
      EXCEPTION("Turret history compression failed: {}", ZSTD_getErrorName(bytes));
    block.raw = bytes >= raw_size;
    if (block.raw) {
      bytes = raw_size;
      memcpy(block.bytes.data(), tail_.data(), bytes);
    }
    block.bytes.resize(bytes);
    block.bytes.shrink_to_fit();
    blocks_.push_back(std::move(block));
    tail_.clear();
  }

  void TurretHistory::push(const TurretData &data, Encoder &encoder) {
    if (tail_.size() == BLOCK_SAMPLES)
      flush(encoder);
    if (tail_.capacity() == 0)
      tail_.reserve(BLOCK_SAMPLES);
    tail_.push_back(data);
    current_index_ = std::numeric_limits<size_t>::max();
  }

  size_t TurretHistory::readBlock(size_t index, TurretData *destination) const {
    if (index == blocks_.size()) {
      memcpy(destination, tail_.data(), tail_.size() * sizeof(TurretData));
      return tail_.size();
    }
    const auto &block = blocks_[index];
    const size_t expected = BLOCK_SAMPLES * sizeof(TurretData);
    if (block.raw) {
      memcpy(destination, block.bytes.data(), expected);
    } else {
      size_t bytes = ZSTD_decompress(destination, expected, block.bytes.data(), block.bytes.size());
      if (ZSTD_isError(bytes) || bytes != expected)
        EXCEPTION("Invalid compressed turret history block");
    }
    return BLOCK_SAMPLES;
  }

  TurretHistory::TimeState TurretHistory::at(size_t index) const {
    const size_t block = index / BLOCK_SAMPLES;
    TurretData data;
    if (block == blocks_.size()) {
      data = tail_[index % BLOCK_SAMPLES];
    } else {
      std::array<TurretData, BLOCK_SAMPLES> decoded;
      readBlock(block, decoded.data());
      data = decoded[index % BLOCK_SAMPLES];
    }
    return {clock_.history()[first_frame_ + static_cast<uint32_t>(index)].time_ms, data};
  }

  const TurretHistory::TimeState *TurretHistory::currState() const {
    if (!hasData())
      return nullptr;
    const size_t frame = clock_.getCurrIndex();
    const size_t index = frame < first_frame_ ? 0 : frame - first_frame_;
    if (current_index_ != index) {
      current_ = at(index);
      current_index_ = index;
    }
    return &current_;
  }

  TurretHistory::TimeState TurretHistory::Iterator::operator*() const {
    const size_t block = index_ / BLOCK_SAMPLES;
    if (block_ != block) {
      owner_->readBlock(block, decoded_.data());
      block_ = block;
    }
    return {owner_->clock_.history()[owner_->first_frame_ + static_cast<uint32_t>(index_)].time_ms,
            decoded_[index_ % BLOCK_SAMPLES]};
  }

} // namespace unit
