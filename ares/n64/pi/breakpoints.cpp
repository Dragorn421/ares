auto PI::Breakpoints::notifyPlannedWrite(u32 dramAddress, u32 writeLength) -> void {
  fprintf(stderr, "notifyPlannedWrite(0x%08X, 0x%08X)\n", dramAddress,
          writeLength);
  u32 length = writeLength + 1;
  u32 start = dramAddress;
  u32 end = dramAddress + length - 1; // inclusive
  for (auto& bp : plannedWriteBreakpoints) {
    if (bp.start <= end && start <= bp.end) {
        bp.callback();
    }
  }
}
