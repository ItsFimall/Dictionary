# Flipper Zero 英语词典应用
[英文版 (English Version)](https://github.com/Lynn3t/Dictionary/README_en.md)
这是一款为 Flipper Zero 设计的离线英语词典应用程序。

## 1. 功能

- **单词搜索**: 快速查询英语单词的定义和音标。
- **搜索历史**: 保存并回顾最近查询过的10个单词，方便复习。
- **关于页面**: 查看应用的作者信息和数据来源。

## 2. 如何编译和安装

本应用使用 Flipper Zero SDK 工具（ufbt）进行构建，基于 **mntm-009** 固件开发，因此需要手动指定 SDK 版本。

编译和安装过程需要 **Python3** 运行环境，请确保您的系统中已正确安装。

#### 步骤

1. **克隆本项目**

   ```
   git clone https://github.com/Lynn3t/Dictionary.git
   cd Dictionary
   ```

2. **安装 ufbt**

   ```
   pip install ufbt
   ```

3. **安装指定的 mntm-009 SDK**

   ```
   ufbt update -t f7 -u https://github.com/Next-Flip/Momentum-Firmware/releases/download/mntm-009/flipper-z-f7-sdk-mntm-009.zip
   ```

4. **编译工程**

   ```
   ufbt
   ```

5. **上传至 Flipper Zero**

   编译完成后，您可以通过 qFlipper 或以下 ufbt 命令将 .fap 文件上传至您的设备：

   ```
   ufbt launch
   ```

## 3. 引用致谢

本应用的词典数据来源于以下优秀项目，特此感谢：

- [**google-10000-english**](https://github.com/first20hours/google-10000-english): 谷歌根据网页语料库整理的最高频的10000个英语单词列表。
- [**WordNet**](https://wordnet.princeton.edu/): 一个由普林斯顿大学创建的大型英语词汇数据库。 (遵循 WordNet License)
- [**The CMU Pronouncing Dictionary (CMUdict)**](http://www.speech.cs.cmu.edu/cgi-bin/cmudict): 一个开源的、包含超过134,000个单词及其发音的数据库。

## 4. 许可证

本应用程序采用 [MIT 许可证](https://www.bing.com/search?q=MIT LICENSE)。请查看 `LICENSE` 文件获取更多信息。