# src/session

本目录存放 simulator session 实现。

这一层是唯一应该拥有外部 simulator process protocol 的地方，例如 Spectre interactive
SKILL stdin/stdout 处理，或 Ngspice batch-mode 子进程调用。

当前实现：

- `spectre_session.cpp`：`spectre +interactive` backend，负责 SKILL handshake、
  参数写入、`(sclRun "all")`、completion 判断和进程停止。
- `ngspice_session.cpp`：Ngspice batch backend，当前支持三个内置任务：RC low-pass
  AC、RC charging TRAN 与电阻分压 DC sweep。它按任务生成 `rc_ac.cir`、
  `rc_tran.cir` 或 `resistor_divider_dc.cir`，调用 `ngspice -b`，读取
  `wrdata v(out)` 文本输出，并分别映射到 `AcResponse`、`TranWaveform` 或
  `DcSweep`。

命名规则：

- 具体 simulator backend 使用稳定仿真器名，例如 `spectre_session.cpp`、
  `ngspice_session.cpp`。
- 不在本目录放业务 metric、objective、penalty 或项目专用 parser。
