#pragma once

namespace su {

// 受支持的仿真器类型。轻量头，只声明枚举本身，供执行层、工具链探测与上层 facade
// 共同使用，避免上层 public 头把底层实现一起带进来。
enum class SimulatorKind {
  kSpectre,
  kNgspice,
};

}  // namespace su
