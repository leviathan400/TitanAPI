# samples/common - shared mission scaffolding

The three small scaffolding headers every TitanAPI mission uses, kept as **one canonical copy** (they used to be
vendored per sample and drifted into four variants):

- `op2_mission.hpp` - the mission-DLL export contract (`ModDesc` / `ModDescEx` / `SaveRegion` / lifecycle
  callbacks / the `Stream` save-load wrapper).
- `op2_log.hpp` - the append-only file logger (`op2::log::line` / `linef` / `ai` / `stamp` / `pid`), flushed
  per line for crash debugging. Set the log file name per mission with `-DOP2LOG_NAME="c<Name>"` (see any
  sample's CMakeLists); the main log is `<name>.log`, the AI channel `<name>-AI.log`.
- `op2_crash.hpp` - SEH guards for the engine entry points + the process-wide unhandled-fault filter.

Samples add this directory to their include path:

```cmake
target_include_directories(c<Name> PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}"
  "${CMAKE_CURRENT_SOURCE_DIR}/../common"
  "${CMAKE_CURRENT_SOURCE_DIR}/../../TitanAPI/include")
```

The one exception is `samples/Layer1`, which deliberately vendors its own **frozen** copies (it is the
Layer-1-only snapshot and must keep building without the live library).
