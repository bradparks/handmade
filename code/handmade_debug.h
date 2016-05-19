#ifndef HANDMADE_DEBUG_H
#define HANDMADE_DEBUG_H

#define TIMED_BLOCK__(Number, ...) timed_block TimedBlock_##Number(__COUNTER__, __FILE__, __LINE__, __FUNCTION__, ## __VA_ARGS__)
#define TIMED_BLOCK_(Number, ...) TIMED_BLOCK__(Number, ## __VA_ARGS__)
#define TIMED_BLOCK(...) TIMED_BLOCK_(__LINE__, ## __VA_ARGS__)

struct debug_record {
    u64 CycleCount;

    char *FileName;
    char *FunctionName;

    u32 LineNumber;
    u32 HitCount;
};

debug_record DebugRecordArray[];

struct timed_block {
    debug_record *Record;

    timed_block(int Counter, char *FileName, int LineNumber, char *FunctionName, int HitCount = 1) {
        // TODO: Thread safety
        Record = DebugRecordArray + Counter;
        Record->FileName = FileName;
        Record->LineNumber = LineNumber;
        Record->FunctionName = FunctionName;
        Record->CycleCount -= __rdtsc();
        Record->HitCount += HitCount;
    }

    ~timed_block() {
        Record->CycleCount += __rdtsc();
    }
};

#endif // HANDMADE_DEBUG_H
