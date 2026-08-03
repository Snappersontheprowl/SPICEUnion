# src/parse

本目录存放结果读取器与底层 result helper 实现。

M2 会先实现最小 ResultIR、通用结果读取 helper 和清晰失败语义。
Python `task_library.py` 只作为历史参考与 fixture 来源；本目录不为了强行兼容 Python
返回习惯而牺牲 C++ API 的类型安全。

完整 netlist IR、业务 parser、objective、penalty、pass/fail 规则不属于本目录职责。
