# rc_charging_tran

这是 M3.3 使用的 Spectre RC charging TRAN 源 fixture。

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
| `Vin` | `1 V` |
| `tran stop` | `10 ns` |
| `maxstep` | `10 ps` |
| 初始 `out` | `0 V` |

理论响应：

```text
v(out, t) = Vin * (1 - exp(-t / (R*C)))
τ = RC = 1 ns
```

生成命令要点：

```bash
spectre tests/fixtures/spectre/rc_charging_tran/input.scs \
  -64 \
  -raw spectre_rc_charging_tran.raw \
  -format psfbin
```

该 fixture 已由本机 Spectre 23.1 生成普通 transient PSF 文件：

```text
tests/fixtures/psf/spectre_rc_charging_tran.raw/tran.tran.tran
```

当前 `SPICEUNION_ENABLE_LIBPSF_READER=ON` 时，`read_tran_waveform()` 可以读取
`out` 信号，并通过与 Ngspice RC TRAN 相同的 `τ` / `5τ` 充电曲线语义检查。
