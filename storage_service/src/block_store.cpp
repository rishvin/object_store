#include <experimental/random>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <mutex>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unordered_set>

const size_t kBlockSize = 1024 * 1024 * 1024;
std::string gDirPath = ".";
const size_t kMaxBlocks = 2;

std::mutex gMutex;

struct Buffer {
  explicit Buffer() : bufSize(0), buffer(nullptr) {}

  void allocate(size_t size) {
    bufSize = size + sizeof(Header);
    buffer = std::make_unique<char[]>(bufSize);

    auto *header = reinterpret_cast<Header *>(buffer.get());
    header->size = bufSize;
    header->stamp = 0x1234;
    header->shouldDelete = false;
  }

  void copy(const std::string &bytes) {
    assert(buffer);
    assert(size() == bytes.size());

    auto buffer = raw();
    memcpy(buffer, bytes.c_str(), bytes.size());
  }

  size_t readFromBlock(FILE *fp) { return fread(buffer.get(), 1, bufSize, fp); }

  size_t writeToBlock(FILE *fp) { return fwrite(buffer.get(), 1, bufSize, fp); }

  size_t size() { return bufSize - sizeof(Header); }

  size_t totalSize() { return bufSize; }

  char *raw() { return buffer.get() + sizeof(Header); }

  struct Header {
    size_t size;
    size_t stamp;
    bool shouldDelete;
  };

  size_t bufSize;
  std::unique_ptr<char[]> buffer;
};

const std::string getBlockId(const std::string id) {
  const std::string blockPrefix = "block";
  return gDirPath + "/" + blockPrefix + id;
}

struct BufferWriter {
  BufferWriter(const std::string &blockId, size_t offset) {
    fp = fopen(blockId.c_str(), "rb+");
    fseek(fp, offset, SEEK_CUR);
  }

  ~BufferWriter() { fclose(fp); }

  size_t write(Buffer &buffer) { return buffer.writeToBlock(fp); }

  BufferWriter &operator=(const BufferWriter &) = delete;
  BufferWriter(const BufferWriter &) = delete;
  BufferWriter() = default;

  FILE *fp;
};

struct BufferReader {
  BufferReader(const std::string &blockId, size_t offset) {
    fp = fopen(blockId.c_str(), "rb");
    fseek(fp, offset, SEEK_CUR);
  }

  ~BufferReader() { fclose(fp); }

  size_t read(Buffer &buffer) { return buffer.readFromBlock(fp); }

  BufferReader &operator=(const BufferReader &) = delete;
  BufferReader(const BufferReader &) = delete;
  BufferReader() = default;

  FILE *fp;
};

struct BlockDescriptor {
  BlockDescriptor(const std::string &blockId, size_t blockSize = kBlockSize)
      : blockId(blockId), offset(0) {
    if (std::filesystem::exists(blockId)) {
      scanBlock();
    } else {
      auto fp = fopen(blockId.c_str(), "wb");
      fclose(fp);
      std::filesystem::resize_file(blockId, blockSize);
    }
  }

  ~BlockDescriptor() {}

  BlockDescriptor(BlockDescriptor &&fs) {
    blockId = fs.blockId;
    offset = fs.offset;

    fs.blockId = "";
    fs.offset = 0;
  }

  void scanBlock() {
    auto fp = fopen(blockId.c_str(), "r");
    Buffer::Header header{0, 0};
    while (!feof(fp)) {
      char *buffer = reinterpret_cast<char *>(&header);
      if (auto readCount = fread(buffer, 1, sizeof(Buffer::Header), fp)) {
        if (readCount != sizeof(Buffer::Header) || header.stamp != 0x1234) {
          break;
        }
        offset += header.size;
        rewind(fp);
        fseek(fp, offset, SEEK_SET);
      } else {
        break;
      }
    }
  }

  BlockDescriptor &operator=(const BlockDescriptor &) = delete;
  BlockDescriptor(const BlockDescriptor &) = delete;
  BlockDescriptor() = default;

  std::string blockId;
  size_t offset;
};

struct BlockRegister {
  void addBlock(BlockDescriptor &&blockDesc) {
    blockList.emplace_back(std::move(blockDesc));
  }

  BlockDescriptor *getBlock(size_t dataSize) {
    std::unordered_set<int> seenIdx;

    std::lock_guard<std::mutex> guard(gMutex);
    while (seenIdx.size() < blockList.size()) {
      int idx =
          std::experimental::randint(0, static_cast<int>(blockList.size()) - 1);
      seenIdx.emplace(idx);

      auto &blockDesc = blockList[idx];
      if (blockDesc.offset + dataSize <= kBlockSize) {
        return &blockDesc;
      }
    }

    return nullptr;
  }

  std::vector<std::string> getBlockIds() {
    std::vector<std::string> blockIds;
    for (auto &block : blockList) {
      blockIds.emplace_back(block.blockId);
    }

    return blockIds;
  }

  std::vector<BlockDescriptor> blockList;
} gBlockRegister;

void initBlockStore(const std::string &path) {
  std::error_code ec;
  gDirPath = path;

  const std::filesystem::space_info spaceInfo =
      std::filesystem::space(gDirPath, ec);

  size_t numBlocks = std::min(kMaxBlocks, spaceInfo.free / kBlockSize);
  size_t kBlockCount = 1;

  if (numBlocks < kBlockCount) {
    size_t spaceGb = spaceInfo.free / 10e9;
    size_t requiredGb = numBlocks + 1;
    std::cout << "Insufficient free space, available: " << spaceGb
              << ", required at least: " << requiredGb << "GB" << std::endl;
    abort();
  }

  for (size_t idx = 1; idx <= numBlocks; ++idx) {
    gBlockRegister.addBlock(BlockDescriptor{getBlockId(std::to_string(idx))});
  }

  std::cout << "Block store initialized" << std::endl;
}

std::tuple<std::string, size_t> reserveForWrite(size_t size) {
  size_t dataSize = size;
  std::string blockId = "";
  size_t offset = -1;

  if (auto desc = gBlockRegister.getBlock(dataSize)) {
    auto &handle = *desc;
    blockId = handle.blockId;
    offset = handle.offset;
    handle.offset += dataSize;
  }

  return std::make_tuple(blockId, offset);
}

std::vector<std::string> getBlockIds() { return gBlockRegister.getBlockIds(); }

void testWrite(const std::string data) {
  Buffer buffer{};
  buffer.allocate(data.size());

  auto [blockId, offset] = reserveForWrite(data.size());
  memcpy(buffer.raw(), data.c_str(), data.size());

  std::cout << "Writing to block: " << blockId << ", offset: " << offset
            << ", size: " << data.size() << std::endl;

  BufferWriter writer{blockId, offset};
  writer.write(buffer);
}

void testRead(std::string blockId, size_t offset, size_t size) {
  BufferReader reader{blockId, offset};
  Buffer readBuffer{};
  readBuffer.allocate(size);
  reader.read(readBuffer);

  char *ch = readBuffer.raw();
  std::cout << "Content: ";
  for (size_t i = 0; i < readBuffer.size(); i++) {
    std::cout << *ch;
    ++ch;
  }
  std::cout << std::endl;
}

class ClientContext {
public:
  ClientContext() : offset{0} {}

  std::tuple<std::string, size_t, size_t> write(const std::string &str) {
    buffer.allocate(str.size());

    auto [blockId, offset] = reserveForWrite(buffer.totalSize());
    if (blockId == "") {
      return std::make_tuple(blockId, offset, 0);
    }

    buffer.copy(str);

    BufferWriter writer{blockId, offset};
    auto written = writer.write(buffer);
    return std::make_tuple(blockId, offset, written);
  }

  pybind11::bytes read(std::string blockId, size_t offset, size_t size) {
    buffer.allocate(size);

    BufferReader reader{blockId, offset};
    reader.read(buffer);
    return std::string(buffer.raw(), buffer.size());
  }

private:
  std::string blockId;
  size_t offset;
  Buffer buffer;
};

PYBIND11_MODULE(storage_service, mod) {
  pybind11::class_<ClientContext>(mod, "ClientContext")
      .def(pybind11::init<>())
      .def("write", &ClientContext::write)
      .def("read", &ClientContext::read);
  mod.def("initBlockStore", &initBlockStore, "Initialize the block store.");
  mod.def("getBlockIds", &getBlockIds, "Get blocks Id");
  mod.def("testWrite", &testWrite, "Test the write path.");
  mod.def("testRead", &testRead, "Test the read path.");
}