# rc_lowpass_ac

这是 M3.2 使用的 Spectre RC low-pass AC 源 fixture。

电路：

```text
Vin -- R1 -- out
             |
             C1
             |
             0
```

参数：

| 参数 | 值 |
|---|---:|
| `R` | `1 kΩ` |
| `C` | `1 pF` |
| `Vin` AC magnitude | `1 V` |
| AC sweep | `1 MHz` 到 `10 GHz`，每 decade 100 点 |

理论 -3 dB 频率：

```text
fc = 1 / (2πRC) ≈ 159.154943 MHz
```

固化结果位置：

```text
tests/fixtures/psf/spectre_rc_lowpass_ac.raw/
```

生成命令要点：

```bash
spectre tests/fixtures/spectre/rc_lowpass_ac/input.scs \
  -64 \
  -raw spectre_rc_lowpass_ac.raw \
  -format psfbin
```

该 fixture 用于验证 Spectre 侧 `read_ac_response()` 读出的 `AcResponse` 能通过与
Ngspice RC AC 相同的语义检查。
