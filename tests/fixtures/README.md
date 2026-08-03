# tests/fixtures

本目录存放测试用固定样本。

fixture 是“为了测试某个功能而准备好的固定输入样本”。在 SPICEUnion 当前阶段，
这里主要存放小体积 Spectre PSF 结果文件，用来验证结果读取器，而不是验证 Spectre
本身能否运行。

## 使用边界

- 本目录中的样本应尽量小、稳定、可重复。
- 可以提交已脱敏或无敏感模型正文的小型 `.raw` 结果目录。
- 不提交大型 corner / monte-carlo / PDK runtime 目录。
- 不把临时仿真输出直接倒进本目录；新增 fixture 前需要记录来源、用途和已知期望值。
- 缺失样本先在对应 README 中标记，不用拿大型不稳定结果硬凑。

## 当前子目录

- `psf/`：Spectre PSF 结果读取相关 fixture。
