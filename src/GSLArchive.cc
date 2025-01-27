#include "GSLArchive.hh"

#include <phosg/Encoding.hh>
#include <phosg/Filesystem.hh>
#include <stdexcept>

#include "Text.hh"

using namespace std;



template <typename LongT>
struct GSLHeaderEntry {
  ptext<char, 0x20> filename;
  LongT offset; // In pages, so actual offset is this * 0x800
  LongT size;
  uint64_t unused;
} __attribute__((packed));

template <typename LongT>
void GSLArchive::load_t() {
  StringReader r(*this->data);
  uint64_t min_data_offset = 0xFFFFFFFFFFFFFFFF;
  while (r.where() < min_data_offset) {
    const auto& entry = r.get<GSLHeaderEntry<LongT>>();
    if (entry.filename.len() == 0) {
      break;
    }
    uint64_t offset = static_cast<uint64_t>(entry.offset) * 0x800;
    if (offset + entry.size > this->data->size()) {
      throw runtime_error("GSL entry extends beyond end of data");
    }
    this->entries.emplace(entry.filename, Entry{offset, entry.size});
  }
}

GSLArchive::GSLArchive(shared_ptr<const string> data, bool big_endian)
  : data(data) {
  if (big_endian) {
    this->load_t<be_uint32_t>();
  } else {
    this->load_t<le_uint32_t>();
  }
}

const unordered_map<string, GSLArchive::Entry> GSLArchive::all_entries() const {
  return this->entries;
}

pair<const void*, size_t> GSLArchive::get(const std::string& name) const {
  try {
    const auto& entry = this->entries.at(name);
    return make_pair(this->data->data() + entry.offset, entry.size);
  } catch (const out_of_range&) {
    throw out_of_range("GSL does not contain file: " + name);
  }
}

string GSLArchive::get_copy(const string& name) const {
  try {
    const auto& entry = this->entries.at(name);
    return this->data->substr(entry.offset, entry.size);
  } catch (const out_of_range&) {
    throw out_of_range("GSL does not contain file: " + name);
  }
}

StringReader GSLArchive::get_reader(const string& name) const {
  try {
    const auto& entry = this->entries.at(name);
    return StringReader(this->data->data() + entry.offset, entry.size);
  } catch (const out_of_range&) {
    throw out_of_range("GSL does not contain file: " + name);
  }
}
