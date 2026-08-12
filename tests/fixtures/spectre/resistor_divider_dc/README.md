# resistor_divider_dc

这是 M3 最小收口使用的 Spectre resistor divider DC sweep 源 fixture。

电路：

```text
Vin -- Rtop -- out -- Rbottom -- 0
```

参数：

| 参数 | 值 |
|---|---:|
| `Rtop` | `3 kΩ` |
| `Rbottom` | `1 kΩ` |
| `vin_dc` sweep | `0 V` 到 `1 V`，步进 `0.1 V` |

理论分压比例：

```text
out = vin_dc * Rbottom / (Rtop + Rbottom) = vin_dc * 0.25
```

固化结果位置：

```text
tests/fixtures/psf/spectre_resistor_divider_dc.raw/
```

生成命令要点：

```bash
spectre tests/fixtures/spectre/resistor_divider_dc/input.scs \
  -64 \
  -raw spectre_resistor_divider_dc.raw \
  -format psfbin
```

该 fixture 用于验证 Spectre 侧 DC sweep 能映射为 `DcSweep`，并与 Ngspice
`kResistorDividerDc` 复用同类分压语义检查。
