#include <n64/n64.hpp>

namespace ares::Nintendo64 {

Cartridge& cartridge = cartridgeSlot.cartridge;
#include "slot.cpp"
#include "flash.cpp"
#include "rtc.cpp"
#include "joybus.cpp"
#include "isviewer.cpp"
#include "debugger.cpp"
#include "serialization.cpp"

auto Cartridge::allocate(Node::Port parent) -> Node::Peripheral {
  return node = parent->append<Node::Peripheral>(string{system.name(), " Cartridge"});
}

auto Cartridge::connect() -> void {
  if(!node->setPak(pak = platform->pak(node))) return;

  information = {};
  information.title  = pak->attribute("title");
  information.region = pak->attribute("region");
  information.cic    = pak->attribute("cic");

  if(auto fp = pak->read("program.rom")) {
    u32 args = fp->size();
    u32 n_args = system.gameArguments.size();
    u32 args_size = 0;
    for (int i = 0; i < n_args; i++)
      args_size += system.gameArguments[i].length() + 1;
    printf("args = 0x%08X\n", args);
    printf("n_args = %d\n", n_args);
    printf("args_size = 0x%08X\n", args_size);

    u32 capacity = fp->size() + args_size;
    capacity = (capacity + 7) & ~7;
    rom.allocate(capacity);

    rom.load(fp);

    // TODO check if rom header is homebrew header, check flags, check version values
    u32 chunks_header_offset = rom.read<Word>(0x14);
    printf("chunks_header_offset = 0x%08X\n", chunks_header_offset);
    u32 n_chunks = rom.read<Word>(chunks_header_offset + 4);
    printf("n_chunks = %d\n", n_chunks);
    u32 args_chunk_offset = 0;
    for (int i = 0; i < n_chunks; i++) {
      u8 name[4];
      for (int j = 0; j < 4; j++)
        name[j] = rom.read<Byte>(chunks_header_offset + 8 + 8 * i + j);
      printf("name = %.*s\n", 4, name);
      if (std::memcmp(name, "ARGS", 4) == 0) {
        args_chunk_offset = rom.read<Word>(chunks_header_offset + 8 + 8 * i + 4);
      }
    }
    printf("args_chunk_offset = 0x%08X\n", args_chunk_offset);
    if (args_chunk_offset != 0) {
      rom.data[args_chunk_offset + 4 + 3] = (args >> 24) & 0xFF;
      rom.data[args_chunk_offset + 4 + 2] = (args >> 16) & 0xFF;
      rom.data[args_chunk_offset + 4 + 1] = (args >> 8) & 0xFF;
      rom.data[args_chunk_offset + 4 + 0] = (args >> 0) & 0xFF;

      rom.data[args_chunk_offset + 8 + 3] = (n_args >> 24) & 0xFF;
      rom.data[args_chunk_offset + 8 + 2] = (n_args >> 16) & 0xFF;
      rom.data[args_chunk_offset + 8 + 1] = (n_args >> 8) & 0xFF;
      rom.data[args_chunk_offset + 8 + 0] = (n_args >> 0) & 0xFF;

      rom.data[args_chunk_offset + 12 + 3] = (args_size >> 24) & 0xFF;
      rom.data[args_chunk_offset + 12 + 2] = (args_size >> 16) & 0xFF;
      rom.data[args_chunk_offset + 12 + 1] = (args_size >> 8) & 0xFF;
      rom.data[args_chunk_offset + 12 + 0] = (args_size >> 0) & 0xFF;
    }

    uint32_t offset = args;
    for (int i = 0; i < n_args; i++) {
      printf("[%d] %s\n", i, system.gameArguments[i].data());
      auto sz = system.gameArguments[i].length() + 1;
      for (int j = 0; j < sz; j++) {
        u32 offset_j = offset + j;
        u32 wordaligned_offset = offset_j / 4 * 4;
        u32 wordswapped_offset = wordaligned_offset + 3 - (offset_j - wordaligned_offset);
        rom.data[wordswapped_offset] = j == sz - 1 ? 0 : system.gameArguments[i][j];
        printf("0x%08X <- 0x%02X\n", wordswapped_offset, rom.data[wordswapped_offset]);
      }
      offset += sz;
    }
  } else {
    rom.allocate(16);
  }

  if(auto fp = pak->read("save.ram")) {
    ram.allocate(fp->size());
    ram.load(fp);
  }

  if(auto fp = pak->read("save.eeprom")) {
    eeprom.allocate(fp->size());
    eeprom.load(fp);
  }

  if(auto fp = pak->read("save.flash")) {
    flash.allocate(fp->size());
    flash.load(fp);
  }

  rtc.load();

  if(rom.size <= 0x03ff'0000) {
    isviewer.ram.allocate(64_KiB);
    isviewer.tracer = node->append<Node::Debugger::Tracer::Notification>("ISViewer", "Cartridge");
    isviewer.tracer->setAutoLineBreak(false);
    isviewer.tracer->setTerminal(true);
  }

  debugger.load(node);

  power(false);
}

auto Cartridge::disconnect() -> void {
  if(!node) return;
  save();
  debugger.unload(node);
  rom.reset();
  ram.reset();
  eeprom.reset();
  flash.reset();
  isviewer.ram.reset();
  pak.reset();
  node.reset();
}

auto Cartridge::save() -> void {
  if(!node) return;

  if(auto fp = pak->write("save.ram")) {
    ram.save(fp);
  }

  if(auto fp = pak->write("save.eeprom")) {
    eeprom.save(fp);
  }

  if(auto fp = pak->write("save.flash")) {
    flash.save(fp);
  }

  rtc.save();
}

auto Cartridge::power(bool reset) -> void {
  flash.mode = Flash::Mode::Idle;
  flash.status = 0;
  flash.source = 0;
  flash.offset = 0;
  isviewer.ram.fill(0);
  rtc.power(reset);
}

}
